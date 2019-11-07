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

#include "manager.hh"
#include <iostream>
#include <thread>
#include <assert.h>
#include <signal.h>
#include <cstring>

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//#include "libtrace.h"

#include "dns_util.hh"
#include "utility.hh"

#include <stdlib.h> //srand rand
#include <time.h>   //time
using namespace std;

#define FAIL_RETRY_LIMIT 5
#define SLEEP_TIME 1.0
#define FAKE_TRACE_START_TIME 1000000000.0

Manager::Manager(int n, bool d, string conn,
		 string in_fn, string in_ft, string out_fn,
		 string c_ip, int c_port,
		 vector<int> clt_fd, vector<int> clt_pid, bool no_map, int l, double pace)
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  
  my_pid = getpid();   //get process ID
  done_sync_time = false;
  disable_mapping = no_map;
  trace_limit = double(l);
  query_pace = pace;
  query_pace_ts = (query_pace > 0 ? FAKE_TRACE_START_TIME : 0);

  //assign input data
  num_clients = n;
  dist = d;
  input_file = in_fn;
  input_format = in_ft;
  if (!dist)
    ins = new InputSource(in_fn, in_ft);
  output_file = out_fn;
  //output_format = out_ft;
  command_ip = c_ip;
  command_port = c_port;
  conn_type = conn;

  client_fd = clt_fd;
  client_pid = clt_pid;
  assert(clt_fd.size() == clt_pid.size());

  client_t *tmp;
  for (unsigned int i=0; i<client_fd.size(); i++) {
    client_fd2pid.insert(make_pair(client_fd[i], client_pid[i]));
    client_idx2fd.insert(make_pair(i, client_fd[i]));
    tmp = new client_t;
    tmp->idx = i;
    tmp->fd = client_fd[i];
    tmp->pid = client_pid[i];
    tmp->bev = NULL;
    client_vec.push_back(tmp);
  }

  //randome seed
  srand(time(NULL));

  //output file
  if (output_file.length() != 0 && output_file != "-") {
    out_fs.open(output_file);
    assert(out_fs.is_open());
  }

  if (dist) {  //fill in commander address,
    com_msg_buffer.clear();
    // please refer https://www.ibm.com/support/knowledgecenter/ssw_ibm_i_72/rzab6/xip6client.htm 
    // for more info (to support both ipv4 and ipv6)

    com_addr = NULL; // intially should set this to null
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    struct sockaddr_in6 serveraddr;
    hints.ai_family = AI_NUMERICSERV;
    hints.ai_family = AF_UNSPEC;

    if(inet_pton(AF_INET, command_ip.c_str(), &serveraddr)){
      // IPv4 address.
      hints.ai_family = AF_INET;
      hints.ai_flags |= AI_NUMERICHOST;
    }

    else if (inet_pton(AF_INET6, command_ip.c_str(), &serveraddr)){
      // IPv6 address.
      hints.ai_family = AF_INET6;
      hints.ai_flags |= AI_NUMERICHOST;
    }

    else{
      // Invalid address.
      err(1, "[error] commander ip [%s] is invalid, abort!", command_ip.c_str());
    }
    int rc = getaddrinfo(command_ip.c_str(), std::to_string(command_port).c_str(), &hints, &com_addr);
    if(rc!=0){
      if(rc==EAI_SYSTEM){
        err(1, "[error] getaddrinfo() failed\n");
      }
    }
    assert(com_addr != NULL);

  }
}

Manager::~Manager()
{
  clean_up();
}

/*
  clean up
*/
void Manager::clean_up()
{
  //if (client_fd_map) delete client_fd_map;
  for (client_t *clt : client_vec) {
    if (clt->bev)
      bufferevent_free(clt->bev);
    delete clt;
  }
  if (ins != NULL)
    delete ins;
  if (out_fs.is_open())
    out_fs.close();
}

/*
  not read or write
*/
static void buf_ev_cb(struct bufferevent *bev, short events, void *ctx)
{
  evutil_socket_t fd = bufferevent_getfd(bev);
  //clt_arg_t *clt = (clt_arg_t *)ctx;
  if (events & events & BEV_EVENT_CONNECTED) {
    LOG(LOG_DBG, "fd [%d] connected\n", (int)fd);
  } else if (events & (BEV_EVENT_EOF|BEV_EVENT_ERROR|BEV_EVENT_TIMEOUT)) {
    if (events & BEV_EVENT_ERROR)
      cerr << "error!!!!["<< EVUTIL_SOCKET_ERROR() <<"]"<<endl;
    DBG("fd[%d] %s%s%s\n",
	(int)fd,
	(events & BEV_EVENT_TIMEOUT) ? "timeout":"",
	(events & BEV_EVENT_EOF) ? "got a close":"",
	(events & BEV_EVENT_ERROR) ? evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()):"");
    bufferevent_free(bev);
  }
}

/*
  signal call back: exit event loop
  clean up is done when the caller returns
*/
static void signal_cb(evutil_socket_t sig, short what, void *arg)
{
  assert(what & EV_SIGNAL);
  struct event_base *base = (struct event_base *)arg;
  struct timeval delay = {2, 0};
  cerr << "[" << getpid() << "] Signal ("
       << sig << ") received, exit in two seconds!\n";
  event_base_loopexit(base, &delay);
}

/*
  helper of read_client_cb
*/
void Manager::read_client_cb_helper(struct bufferevent *bev, void *ctx)
{
  (static_cast<Manager *>(ctx))->read_client_cb(bev);
}

/*
  read data from client
*/
void Manager::read_client_cb(struct bufferevent *bev)
{
  assert(bev);
  int fd = bufferevent_getfd(bev);
  LOG(LOG_DBG, "[%d] read from client [%d] with fd [%d]\n", my_pid, client_fd2pid[fd], fd);

  struct evbuffer *input_buffer = bufferevent_get_input(bev);
  assert(input_buffer);

  size_t len = evbuffer_get_length(input_buffer);

  if (len > 0) {
    char *data = new char[len+1];//+1 makes sure the end of string is '\0'
    assert(data);
    memset(data, '\0', len+1);
    bufferevent_read(bev, data, len);//read len, not +1

    //20170912: do not write to controller for now
    //if (dist) //write to commander in distributed mode
    //  bufferevent_write(com_bev, data, len);

    if (output_file.length() != 0) { //log to local file in non-distributed mode
      if (out_fs.is_open())
	out_fs << data;
      else
	cout << data;
    }
    delete[] data;
  }
}

void Manager::com_read_cb_helper(struct bufferevent *bev, void *ctx)
{
  (static_cast<Manager *>(ctx))->com_read_cb(bev);
}

void Manager::com_read_cb(struct bufferevent *bev)
{
  assert(bev);
  assert(com_bev == bev);
  int fd = bufferevent_getfd(bev);
  LOG(LOG_DBG, "[%d] read from commander fd [%d]\n", my_pid, fd);
  
  struct evbuffer *input_buffer = bufferevent_get_input(bev);
  assert(input_buffer);
  size_t len = evbuffer_get_length(input_buffer);
  uint8_t *data = new uint8_t[len];
  assert(data);
  bufferevent_read(bev, data, len);
  com_msg_buffer.append(reinterpret_cast<const char*>(data), len);
  delete[] data;

  //in dist mode, sync time message should be set in commander not
  //manager since manager does not know the start of the trace; so we
  //don't do sync time message like Manager::read_input_file()
  while (com_msg_buffer.size() > sizeof(uint32_t)) {
    uint8_t *d = (uint8_t *)(com_msg_buffer.data());
    uint32_t sz = 0;
    memcpy(&sz, d, sizeof(uint32_t));
    sz = ntohl(sz);
    uint32_t left = com_msg_buffer.size() - sizeof(uint32_t);
    d += sizeof(uint32_t);

    if (sz > left) break; //not enought data left

    log_dbg("receive message from controller");
    
    //message used to check sync_time and get the src_ip
    trace_replay::DNSMsg *msg = new trace_replay::DNSMsg();
    assert(msg);
    msg->ParseFromArray(d, sz);
    //string raw = msg->raw();
    //print_dns_pkt((uint8_t*)raw.data(), raw.size());

    if (!done_sync_time && msg->sync_time()) {//sync_time message sent to all sub-clients
      log_dbg("recv sync_time from controller");
      done_sync_time = true;
      uint32_t msg_sz = htonl(sz);
      for (int cfd : client_fd) { //write length and then the data
	if (-1 == write(cfd, &msg_sz, sizeof(msg_sz))) log_err("fail to write socket");
	if (-1 == write(cfd, d, sz)) log_err("fail to write socket");
      }
      log_dbg("sent sync time message to all client processes");
    } else { //normal message sent one sub-clients
      int fd = rand_client_fd((char *)(msg->src_ip().c_str()));
      assert(fd != -1);

      LOG(LOG_DBG, "[%d] write to client [%d] with fd [%d]\n", my_pid, client_fd2pid[fd], fd);

      //write length and then the data
      uint32_t ln = htonl(sz);
      if (-1 == write(fd, &ln, sizeof(ln))) log_err("fail to write socket");
      if (-1 == write(fd, d, sz)) log_err("fail to write socket");
    }
    delete msg;
    com_msg_buffer = com_msg_buffer.substr(sz + sizeof(uint32_t));
  }
}

void Manager::com_bevent_cb_helper(struct bufferevent *bev, short which, void *ctx)
{
  int pid = getpid();
  evutil_socket_t fd = bufferevent_getfd(bev);
  string err_msg;
  if (which & BEV_EVENT_CONNECTED) {
    LOG(LOG_DBG, "[%d] commander fd [%d] connected\n", pid, fd);
  } else if (which & BEV_EVENT_ERROR) {
    LOG(LOG_ERR, "[%d] [error] commander fd [%d] error [%d]\n", pid, fd, EVUTIL_SOCKET_ERROR());
    err_msg = evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR());
  } else if (which & (BEV_EVENT_EOF|BEV_EVENT_TIMEOUT)) {
    err_msg = (which & BEV_EVENT_TIMEOUT) ? "timeout" : "got a close";
  }
  if (err_msg.length() != 0) {
    LOG(LOG_ERR, "[%d] [error] commander fd [%d] %s\n", pid, fd, err_msg.c_str());
    bufferevent_free(bev);
    (static_cast<Manager *>(ctx))->com_bevent_cb();
  }
}

//re-connect to manager
//after trying 5 times without success, exit!
void Manager::com_bevent_cb()
{
  com_fail_retry += 1;
  if (com_fail_retry >= FAIL_RETRY_LIMIT)
    log_err("connection to commander fails too many times");
  
  com_bev = bufferevent_socket_new(evbase, -1, BEV_OPT_CLOSE_ON_FREE);
  assert(com_bev);
  if (bufferevent_socket_connect(com_bev, com_addr->ai_addr, com_addr->ai_addrlen)<0) {
    bufferevent_free(com_bev);
    log_err("bufferevent_socket_connect fails");
  }
  bufferevent_setcb(com_bev, &Manager::com_read_cb_helper, NULL, &Manager::com_bevent_cb_helper, this);
  bufferevent_enable(com_bev, EV_READ|EV_WRITE);
}

/*
  read input file
*/
void Manager::read_input_file()
{
  if (input_file.length() == 0)
    log_err("input file is invalid");

  if (input_src_map.find(input_format) == input_src_map.end())
    log_err("input format is invalid");

  LOG(LOG_DBG, "[%d] read from input %s file\n", my_pid, input_format.c_str());
    
  trace_replay::DNSMsg *msg = NULL;
  int fd = -1;
  string raw;
  string msg_str;
  double real_start_ts = -1.0;
  double trace_start_ts = -1.0;
  while((msg = ins->get()) != NULL) {
    assert(msg);
    raw = msg->raw();
    if (raw.size() == 0) {//not a valid query
      delete msg;
      msg = NULL;
      continue;
    }
    //print_dns_pkt((uint8_t*)raw.data(), raw.size());

    if (query_pace > 0 && query_pace_ts > 0) { //modify the time in trace
      msg->set_seconds((int)query_pace_ts);
      msg->set_microseconds((query_pace_ts - (int)(query_pace_ts)) * 1000000);
      //msg->set_seconds(query_pace_ts);
      //msg->set_microseconds(0);
      query_pace_ts += query_pace;
    }
    
    //the first message is sent to all client processes to sync time
    if (!done_sync_time) {
      msg->set_sync_time(true);
      msg_str.clear();
      if (!(msg->SerializeToString(&msg_str))) 
	log_err("serialize message fails");
      assert(msg_str.size() > 0);

      done_sync_time = true;
      for (int cfd : client_fd) {
	//write length and then the data
	uint32_t sz = msg_str.size();
	sz = htonl(sz);
	if (-1 == write(cfd, &sz, sizeof(sz))) log_err("fail to write socket");
	if (-1 == write(cfd, msg_str.data(), msg_str.size())) log_err("fail to write socket");
      }
      log_dbg("sent sync time message to all client processes");
    }

    if (trace_limit > 0) {
      double trace_current_ts = double(msg->seconds()) + double(msg->microseconds())/1000000.0;
      if (trace_start_ts < 0) {//the first packet
	trace_start_ts = trace_current_ts;
	real_start_ts = get_time_now("second");
      } else {
	//use get_time_now inside while condition may be necessary
	//for sensitive time accuracy
	while (
	       (trace_current_ts - trace_start_ts)     //trace_ts_diff
	       -
	       (get_time_now("second") - real_start_ts)//real_ts_diff
	       >
	       (trace_limit + double(SLEEP_TIME))      //limit_ts
	       ) {
	  //printf("go to sleep: %.6f\n", get_time_now("second"));
	  sleep(SLEEP_TIME); //wait for SLEEP_TIME
	}
      }
    }

    msg->set_sync_time(false);
    msg_str.clear();
    if (!(msg->SerializeToString(&msg_str))) 
      log_err("serialize message fails");
    assert(msg_str.size() > 0);
    
    fd = rand_client_fd((char *)(msg->src_ip().c_str()));
    assert(fd != -1);

    LOG(LOG_DBG, "[%d] write to client [%d] with fd [%d]\n", my_pid, client_fd2pid[fd], fd);

    //write length and then the data
    uint32_t sz = msg_str.size();
    sz = htonl(sz);
    if (-1 == write(fd, &sz, sizeof(sz))) log_err("fail to write socket");
    if (-1 == write(fd, msg_str.data(), msg_str.size())) log_err("fail to write socket");

    //clean up
    raw.clear();
    msg_str.clear();
    delete msg;
    fd = -1;
  }
}

void Manager::main_event_loop()
{
  evbase = event_base_new();
  if (!evbase)
    log_err("couldn't open event base");

  //set up signal event
  struct event *signal_event = evsignal_new(evbase, SIGINT, signal_cb, (void *)evbase);
  assert(signal_event != NULL);
  if (event_add(signal_event, NULL) < 0)
    log_err("cannot add signal event");

  struct event *sigterm_event = evsignal_new(evbase, SIGTERM, signal_cb, (void *)evbase);
  assert(sigterm_event != NULL);
  if (event_add(sigterm_event, NULL) < 0)
    log_err("cannot add sigterm event");

  //set up buffer event from every client fd
  log_dbg("set up client bufferevent");
  for (client_t *clt : client_vec) {
    clt->bev = bufferevent_socket_new(evbase, clt->fd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(clt->bev, &Manager::read_client_cb_helper, NULL, buf_ev_cb, this);
    bufferevent_enable(clt->bev, EV_READ|EV_WRITE);
  }

  //connect to commander in distributed mode
  if (dist) {
    log_dbg("set up commander bufferevent");
    com_bev = bufferevent_socket_new(evbase, -1, BEV_OPT_CLOSE_ON_FREE);
    assert(com_bev);
    if (bufferevent_socket_connect(com_bev, com_addr->ai_addr, com_addr->ai_addrlen)<0) {
      bufferevent_free(com_bev);
      log_err("bufferevent_socket_connect fails");
    }
    bufferevent_setcb(com_bev, &Manager::com_read_cb_helper, NULL, &Manager::com_bevent_cb_helper, this);
    bufferevent_enable(com_bev, EV_READ|EV_WRITE);
  }

  //start event loop
  log_dbg("start main event loop");
  event_base_dispatch(evbase);
  
  //clean up
  if (dist) {
    bufferevent_free(com_bev);
  }
  for (client_t *clt : client_vec) {
    bufferevent_free(clt->bev);
    clt->bev = NULL;
  }
  event_free(signal_event);
  event_free(sigterm_event);
  event_base_free(evbase);

  if (out_fs.is_open()) {
    out_fs.close();
    log_dbg("close output file");
  }
}

void Manager::start()
{
  log_dbg("manager starts");

  if (dist) {
    //distributed mode: one event base handles all sockets

    log_dbg("distributed mode: read input from commander over TCP");
    main_event_loop();

  } else {
    //non-distributed mode: two threads, one thread read file, the
    //other thread has a event base to handle sub-client communication

    log_dbg("non-distributed mode: read input from local file");
    log_dbg("create thread to read input");
    thread th_read_input(&Manager::read_input_file, this);
    log_dbg("create thread to read client");
    thread th_recv_client(&Manager::main_event_loop, this);
    
    th_recv_client.join();
    th_read_input.join();
  }

  log_dbg("manager ends");
}

/*
  get a random client's fd if src_ip has not been seen
  otherwise, pick the existing one
*/
int Manager::rand_client_fd(char *src_ip)
{
  if (disable_mapping)
    return client_idx2fd[rand() % num_clients];
  
  string ip = src_ip; 
  if (client_src2fd.find(ip) == client_src2fd.end()) {
    int idx = rand() % num_clients;
    int fd = client_idx2fd[idx];
    client_src2fd.insert(make_pair(ip, fd));
    return fd;
  } else
    return client_src2fd[ip];
}

/*
  SIGTERM child processes
*/
void Manager::terminate_clients()
{
  for (pid_t pid : client_pid) {
    int r = kill(pid, SIGTERM);
    if (r == -1)
      warn("[%d] cannot SIGTERM child [%d]", my_pid, pid);
    else
      warnx("[%d] successfully SIGTERM child [%d]", my_pid, pid);
  }  
}

void Manager::log_err(const char *s)
{
  terminate_clients();
  clean_up();
  err(1, "[%d] [error] %s, abort!", my_pid, s);
}

void Manager::log_errx(const char *s)
{
  terminate_clients();
  clean_up();
  errx(1, "[%d] [error] %s, abort!", my_pid, s);
}

void Manager::log(int level, const char *s)
{
  LOG(level, "[%d] %s\n", my_pid, s);
}

void Manager::log_dbg(const char *s)
{
  LOG(LOG_DBG, "[%d] %s\n", my_pid, s);
}

void Manager::log_warnx(const char *s)
{
  warnx("[%d] [warn] %s", my_pid, s);
}

void Manager::log_warn(const char *s)
{
  warn("[%d] [warn] %s", my_pid, s);
}
