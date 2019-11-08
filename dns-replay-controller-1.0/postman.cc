/*
 * Copyright (C) 2018 by the University of Southern California
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include "postman.h"
#include "reader.h"
#include <iostream>
#include <cstring>
#include <cassert>
#include <csignal>
#include <err.h>
#include "dns_util.hh"

#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "addr_util.h"
#include <ctime>   //time
#include "dns_msg.pb.h"

#include <glog/logging.h>

using namespace std;

#define MAX_BUF_SIZE    4096
#define MIN_BUF_SIZE    64

const int random_seed = 1000;

Postman::Postman(string ofn, string ip, int port, int n, int s, Filter &f)
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  
  my_pid = getpid();
  output_file = ofn;
  listen_ip = ip;
  listen_port = port;
  num_clients = n;
  skt = s;
  skt_buffer.clear();

  client_filter.Set(f.GetFilterInput(), f.GetNumClients());
  if (client_filter.GetNumClients() != 0 &&
      !client_filter.GetFilterInput().empty() &&
      (!client_filter.Valid() || n != client_filter.GetNumClients())) {
    errx(1, "[error] client filter is invalid");
  }
  client_filter.Print();
  
  if (listen_port <=0) errx(1, "[error] listen port is invalid");
  if (listen_ip.length() == 0) errx(1, "[error] listen ip is invalid");
  if (num_clients == 0) errx(1, "[error] client number is invalid");

  //random
  mt_generator = mt19937(random_seed);
  uniform_filter = uniform_int_distribution<int>(0, kFilterTotalWeight-1);
  uniform_client = uniform_int_distribution<int>(0, num_clients-1);

  //output file
  if (output_file.length() != 0 && output_file != "-") {
    out_fs.open(output_file);
    assert(out_fs.is_open());
  }
}

Postman::~Postman()
{
  if (out_fs.is_open())
    out_fs.close();
  freeaddrinfo(listen_addr);
}

static void signal_cb(evutil_socket_t sig, short what, void *arg)
{
  assert(what & EV_SIGNAL);
  struct event_base *base = (struct event_base *)arg;
  struct timeval delay = {2, 0};
  cerr << "["<< getpid() << "]" << " Signal (" << sig
       << ") received, exit in two seconds!\n";
  event_base_loopexit(base, &delay);
}

static void accept_error_cb(struct evconnlistener *listener, void *ctx)
{
  struct event_base *base = evconnlistener_get_base(listener);
  int err = EVUTIL_SOCKET_ERROR();
  cerr << "Error on listener " << evutil_socket_error_to_string(err) << endl
       << "shutting down" << endl;
  event_base_loopexit(base, NULL);
}

void Postman::client_bevent_cb_helper(struct bufferevent *bev, short which, void *ctx)
{
  int pid = getpid();
  evutil_socket_t fd = bufferevent_getfd(bev);
  string err_msg;
  if (which & BEV_EVENT_CONNECTED) {
    VLOG(3) << "[" << pid << "] connected fd=" << fd;
  } else if (which & BEV_EVENT_ERROR) {
    LOG(ERROR) << "[" << pid << "] fd=" << fd << " error=" << EVUTIL_SOCKET_ERROR();
    err_msg = evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR());
  } else if (which & (BEV_EVENT_EOF|BEV_EVENT_TIMEOUT)) {
    err_msg = (which & BEV_EVENT_TIMEOUT) ? "timeout" : "got a close";
  }
  if (err_msg.length() != 0) {
    LOG(ERROR) << "[" << pid << "] fd=" << fd << " error:" << err_msg;
    bufferevent_free(bev);

    //need to clean up this fd and client
    (static_cast<Postman *>(ctx))->client_bevent_cb(fd);
  }
}

//delete this client from record
void Postman::client_bevent_cb(int fd)
{
  VLOG(3) << "[" << my_pid << "] clean up fd=" << fd << " from client_fd2bev";
  if (client_fd2bev.find(fd) == client_fd2bev.end())
    errx(1, "[error] cannot find [%d] in record", fd);
  client_fd2bev.erase(fd);

  if (client_fd2src.find(fd) == client_fd2src.end())
    return;

  VLOG(3) << "[" << my_pid << "] clean up fd=" << fd << " from client_src2fd";

  //log stats
  int idx = -1;
  for (unsigned int i=0; i<client_fds.size(); i++) {
    if (client_fds[i] == fd) {
      idx = i;
      break;
    }
  }
  VLOG(1) << "[" << my_pid << "] client id=" << idx <<" fd=" << fd
	  << " total IP=" << client_fd2src[fd].size() << " clean up";

  for (string s : client_fd2src[fd]) {
    client_src2fd.erase(s);
  }
  client_fd2src.erase(fd);
}

/*
  helper of skt_read_cb
*/
void Postman::skt_read_cb_helper(struct bufferevent *bev, void *ctx)
{
  (static_cast<Postman *>(ctx))->skt_read_cb(bev);
}

/*
  read from input socket
*/
void Postman::skt_read_cb(struct bufferevent *bev)
{
  assert(bev);
  int fd = bufferevent_getfd(bev);
  assert(fd == skt);
  VLOG(3) << "[" << my_pid << "] read from input socket";

  struct evbuffer *input_buffer = bufferevent_get_input(bev);
  assert(input_buffer);
  size_t len = evbuffer_get_length(input_buffer);
  uint8_t *data = new uint8_t[len];
  assert(data);
  bufferevent_read(bev, data, len);
  skt_buffer.append(reinterpret_cast<const char*>(data), len);
  delete[] data;

  string raw;
  string msg_str;
  trace_replay::DNSMsg *msg = NULL;
  do {
    uint8_t *d = (uint8_t *)(skt_buffer.data());
    uint32_t sz = 0;
    memcpy(&sz, d, sizeof(uint32_t));
    sz = ntohl(sz);
    uint32_t left = skt_buffer.size() - sizeof(uint32_t);
    if (sz > left) break; //not enought data left
    d += sizeof(uint32_t);
    
    //creat a new message
    msg = new trace_replay::DNSMsg();
    assert(msg);
    msg->ParseFromArray(d, sz);
    
    skt_buffer = skt_buffer.substr(sz + sizeof(uint32_t));

    raw = msg->raw();
    if (raw.size() == 0) {//not a valid query
      delete msg;
      msg = NULL;
      continue;
    }

    //LOG(LOG_DBG, "[%d] input fd [%d] connected\n", my_pid, fd);
    //print_dns_pkt((uint8_t*)raw.data(), raw.size());

    //the first message is sent to all clients to sync time
    if (!done_sync_time) {
      msg->set_sync_time(true);
      msg_str.clear();
      if (!(msg->SerializeToString(&msg_str)))
	err(1, "[error] serialize message fails");
      assert(msg_str.size() > 0);
      uint32_t tmp_sz = htonl(msg_str.size());
      done_sync_time = true;
      for (auto it : client_fd2bev) {
	bufferevent_write(it.second, &tmp_sz, sizeof(tmp_sz));
	bufferevent_write(it.second, msg_str.data(), msg_str.size());
      }
      VLOG(3) << "[" << my_pid << "] sent sync time message to all clients";
    }

    //the message is sent again as normal message
    msg->set_sync_time(false);
    msg_str.clear();
    if (!(msg->SerializeToString(&msg_str)))
      err(1, "[error] serialize message fails");

    assert(msg_str.size() > 0);

    //get client idx and write to it
    bev = rand_client(msg->src_ip().c_str());
    assert(bev);
    fd = bufferevent_getfd(bev);
    assert(fd != -1);
    VLOG(3) << "[" << my_pid << "] write to client fd=" << fd;

    //write length and then the data
    uint32_t tmp_sz = htonl(msg_str.size());
    bufferevent_write(bev, &tmp_sz, sizeof(tmp_sz));
    bufferevent_write(bev, msg_str.data(), msg_str.size());

    //clean up
    delete msg;
    raw.clear();
    msg_str.clear();
    
  } while(skt_buffer.size() > sizeof(uint32_t));
}

/*
  input socket event helper
*/
void Postman::skt_event_cb_helper(struct bufferevent *bev, short which, void *ctx)
{
  (static_cast<Postman *>(ctx))->skt_event_cb(bev, which);
}

/*
  input socket event callback
*/
void Postman::skt_event_cb(struct bufferevent *bev, short which)
{
  evutil_socket_t fd = bufferevent_getfd(bev);
  assert(fd == skt);
  string err_msg;
  if (which & BEV_EVENT_CONNECTED) {
    //this should not happen currently since unix socket is used for
    //reader-postman communication
    VLOG(3) << "[" << my_pid << "] connected input fd=" << fd;
  } else if (which & BEV_EVENT_ERROR) {
    LOG(ERROR) << "[" << my_pid << "] input fd=" << fd << " error=" << EVUTIL_SOCKET_ERROR(); 
    err_msg = evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR());
  } else if (which & (BEV_EVENT_EOF|BEV_EVENT_TIMEOUT)) {
    err_msg = (which & BEV_EVENT_TIMEOUT) ? "timeout" : "got a close";
  }
  if (err_msg.length() != 0) {
    LOG(ERROR) << "[" << my_pid << "] input fd=" << fd << " error:" << err_msg;
    bufferevent_free(bev);
  }
}

void Postman::start()
{
  struct evconnlistener *listener = NULL;

  //create event base
  base = event_base_new();
  if (!base)
    err(1, "cannot open event base");

  //set up signal event
  struct event *signal_event = evsignal_new(base, SIGINT, signal_cb, (void *)base);
  assert(signal_event);
  if (event_add(signal_event, NULL) < 0)
    err(1, "cannot add signal event");

  struct event *sigterm_event = evsignal_new(base, SIGTERM, signal_cb, (void *)base);
  assert(sigterm_event);
  if (event_add(sigterm_event, NULL) < 0)
    err(1, "cannot add sigterm event");

  //set up input socket event
  VLOG(3) << "[" << my_pid << "] set up buffer event for input socket"; 
  skt_bev = bufferevent_socket_new(base, skt, BEV_OPT_CLOSE_ON_FREE);
  assert(skt_bev);
  bufferevent_setcb(skt_bev, &Postman::skt_read_cb_helper, NULL, &Postman::skt_event_cb_helper, this);
  bufferevent_enable(skt_bev, EV_READ|EV_WRITE);
  
  //set listen address
  fill_addr(&listen_addr, listen_ip.c_str(), listen_port);

  //create listener
  listener = evconnlistener_new_bind(base, accept_conn_cb_helper, this,
				     LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
				     listen_addr->ai_addr,
				     listen_addr->ai_addrlen);
  if (!listener)
    err(1, "[error] cannot create listener");
  
  evconnlistener_set_error_cb(listener, accept_error_cb);
  event_base_dispatch(base);

  //clean up
  bufferevent_free(skt_bev);
  evconnlistener_free(listener);
  event_free(signal_event);
  event_free(sigterm_event);
  for (auto it : client_fd2bev) {
    bufferevent_free(it.second);
  }
  event_base_free(base);

  //final log for stats
  for (unsigned int i=0; i<client_fds.size(); i++) {
    if (!client_fd2src.count(client_fds[i])) {
      LOG(WARNING) << "cannot find fd=" << client_fds[i] << " in the record, cleaned up due to client close?";
      continue;
    }
    VLOG(1) << "[" << my_pid << "] client id=" << i <<" fd=" << client_fds[i]
	    << " total IP=" << client_fd2src[client_fds[i]].size();
  }  
}

void Postman::client_read_cb_helper(struct bufferevent *bev, void *ctx)
{
  (static_cast<Postman *>(ctx))->client_read_cb(bev);
}

void Postman::client_read_cb(struct bufferevent *bev)
{
  assert(bev);
  int fd = bufferevent_getfd(bev);
  VLOG(3) << "[" << my_pid << "] read from client fd=" << fd;

  struct evbuffer *input_buffer = bufferevent_get_input(bev);
  assert(input_buffer);

  size_t len = evbuffer_get_length(input_buffer);
  char *data = new char[len];
  assert(data);
  memset(data, 0, len);
  bufferevent_read(bev, data, len);
  
  if (output_file.length() != 0) {
    if (out_fs.is_open())
      out_fs << data;
    else
      cout << data;
  }
  delete[] data;
}

void Postman::accept_conn_cb_helper (struct evconnlistener *listener,
				       evutil_socket_t fd,
				       struct sockaddr *address, int socklen, void *ctx)
{
  (static_cast<Postman *>(ctx))->accept_conn_cb(listener, fd, address, socklen);
}

void Postman::accept_conn_cb (struct evconnlistener *listener,
				evutil_socket_t fd,
				struct sockaddr *address, int socklen)
{
  /*if (evutil_make_socket_nonblocking(fd) < 0) {
    evutil_closesocket(fd);
    err(1, "[error] evutil_make_socket_nonblocking fails");
  }*/

  //get incoming src ip
  char *ip = NULL;
  int l = get_in_addr(address, &ip);
  if (l == -1 || ip == NULL)
    warn("[warn] fail to get the ip address for new connected fd [%d]", fd);

  //get incoming src port
  uint16_t port = get_in_port(address);

  //log info
  VLOG(3) << "[" << my_pid << "] new connection fd=" << fd << " from ip=" << ip << " port=" << port;

  //use src ip and port as key
  string k = ip;
  k += to_string(port);
    
  //delete ip created in get_in_addr
  delete[] ip;
  
  //set up buffer event
  struct event_base *base = evconnlistener_get_base(listener);
  struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
  bufferevent_setcb(bev, client_read_cb_helper, NULL, client_bevent_cb_helper, this);
  bufferevent_enable(bev, EV_READ|EV_WRITE);

  //record this client
  client_fd2bev[fd] = bev;
  client_fd2src[fd] = vector<string> ();
  client_fds.push_back(fd);
  
  //if all clients are connected, start to distribute the input data
  VLOG(3) << "[" << my_pid << "] total connected clients=" << client_fd2bev.size();
  if (!start_read_input && client_fd2bev.size() == num_clients) {
    //read_input();
    string flag = POSTMAN_FLAG;
    write(skt, flag.c_str(), flag.length());
    start_read_input = true;
  }
}

//return the client bev for this src ip
struct bufferevent *Postman::rand_client(const char *src_ip)
{
  string ip = src_ip;
  if (client_src2fd.find(ip) == client_src2fd.end()) {
    //not found, get one fd and bev based on client fileter or randomly
    if (client_fd2bev.size() == 0) {
      errx(1, "[error] no more clients left");
    }
    int idx = 0;
    if (client_filter.GetNumClients() != 0) { //client filter is set
      idx = client_filter.GetClientIndex(uniform_filter(mt_generator));
      VLOG(3) << "[" << my_pid << "] select client fd=" << client_fds[idx] <<" by filter";
    } else {
      idx = uniform_client(mt_generator);
      VLOG(3) << "[" << my_pid << "] select client fd=" << client_fds[idx] <<" randomly";
    }
    if (idx < 0 || (static_cast<unsigned int>(idx) >= client_fds.size())) {
      errx(1, "[error] client idx[%d] out of range client_fds.size()=%lu", idx, client_fds.size());
    }
    client_src2fd[ip] = client_fds[idx];
    client_fd2src[client_fds[idx]].push_back(ip);
    VLOG(2) << "map: " << ip << " " << idx;
  }
  return client_fd2bev[client_src2fd[ip]];
}
