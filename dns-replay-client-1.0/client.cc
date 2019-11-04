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

#include "client.hh"
#include <iostream>
#include <cstring>
#include <cassert>
#include <csignal>
//#include <cstdint>
#include <climits>
#include "global_var.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <event2/listener.h>
#include <event2/bufferevent_ssl.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <openssl/err.h>

#include "dns_util.hh"
#include "utility.hh"
#include "dns_msg.pb.h"
using namespace std;

#define MAX_BUF_SIZE    4096
#define MIN_BUF_SIZE    64

#define UDP_READ_TIMEOUT 60

struct cb_arg_t
{
  void *obj;  // the object pointer used to all the member function
  void *a;    // the additional argument to pass
  void *e;    // additional argument to pass (event pointer?)
  long long unsigned int c;//query count
};

void copy_ts (struct timeval *a, struct timeval *b)
{
  a->tv_sec = b->tv_sec;
  a->tv_usec = b->tv_usec;
}

void recheck_now_ts (struct timeval *t)
{
  evutil_timerclear(t);
  evutil_gettimeofday(t, NULL);
}

/*
  fill in address information
*/
void fill_addr(struct sockaddr_storage *addr, const char *ip, int port)
{
  memset(addr, 0, sizeof(struct sockaddr_storage));
  struct sockaddr_in temp;
  if(inet_pton(AF_INET, ip, &temp)){
    addr->ss_family = AF_INET;
    inet_pton(AF_INET, ip, addr->__ss_padding + sizeof(in_port_t));
    memcpy(addr->__ss_padding, &port, sizeof(uint16_t));
  }
  else if(inet_pton(AF_INET6, ip, &temp)){
    addr->ss_family = AF_INET6;
    inet_pton(AF_INET6, ip, addr->__ss_padding + sizeof(in_port_t)+sizeof(uint32_t));
    memcpy(addr->__ss_padding, &port, sizeof(uint16_t));
  }
  else{
    err(1, "[error] ip[%s] is invalid, abort!", ip);
    return;
  }
}

DNSClient::DNSClient(string c, string g, int t, string s_ip, int s_port, int fd, uint32_t skt_unify, uint32_t opt, bool nw)
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  
  my_pid = getpid();
  server_port = s_port;
  time_out = t;
  server_ip = s_ip;
  conn_type = c;
  manager_fd = fd;
  msg_buffer.clear();
  num_query = 0;
  socket_unify = skt_unify;
  output_option = opt;
  non_wait = nw;
  nagle_option = g;
  fill_addr(&server_addr, server_ip.c_str(), server_port);

  //check input
  if (server_port < 0) log_err("server port must be > 0"); 
  if (time_out < 0) log_err("time out value must be >= 0");
  if (server_ip.length() == 0) log_err("server IP is invalid");
  if (conn_set.find(conn_type) == conn_set.end()) log_err("connection type is invalid");
  if (manager_fd <= 0) log_err("manager fd is invalid");

  if (conn_type == "tls") {
    init_ssl();
  }
}

DNSClient::~DNSClient()
{
  if (manager_fd != -1) {
    close(manager_fd);
  }
  for (auto it : udp_read_event) {
    close(it.first);
    event_free(it.second);
  }
  if (unified_udp_fd != -1) {
    close(unified_udp_fd);
  }
  if (unified_udp_read_event) {
    event_free(unified_udp_read_event);
  }
  if (ssl_ctx) {
    SSL_CTX_free(ssl_ctx);
  }
}

void DNSClient::init_ssl()
{
  SSL_library_init();
  ERR_load_crypto_strings();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();
}

SSL_CTX *DNSClient::get_ssl_ctx()
{
  if (!ssl_ctx) {
    ssl_ctx = SSL_CTX_new(SSLv23_client_method());
  }
  if (!ssl_ctx) {
    ERR_print_errors_fp (stderr);
  }
  return ssl_ctx;
}

struct event_base *DNSClient::get_base()
{
  return base;
}

long long unsigned int DNSClient::get_pending_event_max()
{
  return num_pending_event_max;
}

long long unsigned int DNSClient::get_num_timer()
{
  return num_timer;
}

long long unsigned int DNSClient::get_num_notimer()
{
  return num_notimer;
}

/*
  callback function for signal: exit event loop
*/
static void signal_cb(evutil_socket_t sig, short what, void *ctx)
{
  assert(what & EV_SIGNAL);
  struct event_base *base = (static_cast<DNSClient *>(ctx))->get_base();
  struct timeval delay = {2, 0};
  LOG(LOG_INFO, "[%d] Signal (%d) received, exit in two seconds!\n", getpid(), sig);
  event_base_loopexit(base, &delay);
  LOG(LOG_INFO, "[%d] Max pending event: %llu\n", getpid(), (static_cast<DNSClient *>(ctx))->get_pending_event_max());
  LOG(LOG_INFO, "[%d] Number of timer:   %llu\n", getpid(), (static_cast<DNSClient *>(ctx))->get_num_timer());
  LOG(LOG_INFO, "[%d] Number of notimer: %llu\n", getpid(), (static_cast<DNSClient *>(ctx))->get_num_notimer());
}

/*
  start the main event loop
*/
void DNSClient::start()
{
  signal(SIGPIPE, SIG_IGN);

  //create event base
  base = event_base_new();
  if (!base)
    log_err("couldn't open event base");

  //set up signal event
  struct event *signal_event = evsignal_new(base, SIGINT, signal_cb, this);
  assert(signal_event != NULL);
  if (event_add(signal_event, NULL) < 0) {
    event_free(signal_event);
    log_err("cannot add signal event");
  }

  struct event *sigterm_event = evsignal_new(base, SIGTERM, signal_cb, this);
  assert(sigterm_event != NULL);
  if (event_add(sigterm_event, NULL) < 0) {
    event_free(sigterm_event);
    log_err("cannot add sigterm event");
  }
  //set up buffer event for manager fd
  log_dbg("set up buffer event for manager fd");
  manager_bev = bufferevent_socket_new(base, manager_fd, BEV_OPT_CLOSE_ON_FREE);
  assert(manager_bev);
  bufferevent_setcb(manager_bev, &DNSClient::manager_read_cb_helper, NULL, &DNSClient::manager_event_cb_helper, this);
  bufferevent_enable(manager_bev, EV_READ|EV_WRITE);

  //check if unified udp socket is used
  if (socket_unify & SOCKET_UNIFY_UDP) {
    if ((unified_udp_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	log_err("socket");
    LOG(LOG_DBG, "[%d] unified_udp_fd [%d]\n", my_pid, unified_udp_fd);
    if (evutil_make_socket_nonblocking(unified_udp_fd) < 0) {
      evutil_closesocket(unified_udp_fd);
      log_err("evutil_make_socket_nonblocking fails: unified_udp_fd");
    }
    if (connect (unified_udp_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
      log_err("connect");
    LOG(LOG_DBG, "[%d] create unified_udp_fd [%d]\n", my_pid, unified_udp_fd);

    unified_udp_read_event = event_new(base, unified_udp_fd, EV_READ|EV_PERSIST, &DNSClient::server_udp_read_cb_helper, this);
    assert(unified_udp_read_event != NULL);
    if (event_add(unified_udp_read_event, NULL) < 0){
      event_free(unified_udp_read_event);
      log_err("cannot add unified_udp_read_event");
    } else
      log_dbg("done unified_udp_read_event");
  }
  
  //start event loop
  log_dbg("start client event loop");
  event_base_dispatch(base);

  //clean up
  if (manager_bev)
    bufferevent_free(manager_bev);
  if (signal_event)
    event_free(signal_event);
  if (sigterm_event)
    event_free(sigterm_event);
  event_base_free(base);

  LOG(LOG_DBG, "[%d] num_udp_fd_max=%llu\n", my_pid, num_udp_fd_max);
}

/*
  helper function for manager event callback
*/
void DNSClient::manager_event_cb_helper(struct bufferevent *bev, short which, void *ctx)
{
  (static_cast<DNSClient *>(ctx))->manager_event_cb(bev, which);
}

/*
  manager event: not read and not write
*/
void DNSClient::manager_event_cb(struct bufferevent *bev, short which)
{
  evutil_socket_t fd = bufferevent_getfd(bev);
  assert(fd == manager_fd);
  string err_msg;
  if (which & BEV_EVENT_CONNECTED) {
    //this should not happen currently since unix socket is used for
    //manager-client communication
    LOG(LOG_DBG, "[%d] manager fd [%d] connected\n", my_pid, fd);
  } else if (which & BEV_EVENT_ERROR) {
    LOG(LOG_ERR, "[%d] [error] manager fd [%d] error code [%d]", my_pid, fd, EVUTIL_SOCKET_ERROR());
    err_msg = evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR());
  } else if (which & (BEV_EVENT_EOF|BEV_EVENT_TIMEOUT)) {
    err_msg = (which & BEV_EVENT_TIMEOUT) ? "timeout" : "got a close";
  }
  if (err_msg.length() != 0) { //serious error, socket for manager should not timeout or colse
    LOG(LOG_ERR, "[%d] manager fd [%d] %s", my_pid, fd, err_msg.c_str());
    bufferevent_free(bev);
  }
}

/*
  helper of manager_read_cb
*/
void DNSClient::manager_read_cb_helper(struct bufferevent *bev, void *ctx)
{
  (static_cast<DNSClient *>(ctx))->manager_read_cb(bev);
}

/*
  read from manager
*/
void DNSClient::manager_read_cb(struct bufferevent *bev)
{
  assert(bev);
  int fd = bufferevent_getfd(bev);
  assert(fd == manager_fd);
  LOG(LOG_DBG, "[%d] read from manager fd [%d]\n", my_pid, manager_fd);

  struct evbuffer *input_buffer = bufferevent_get_input(bev);
  assert(input_buffer);
  size_t len = evbuffer_get_length(input_buffer);
  uint8_t *data = new uint8_t[len];
  assert(data);
  bufferevent_read(bev, data, len);
  msg_buffer.append(reinterpret_cast<const char*>(data), len);
  delete[] data;

  //there might be multiple queries (raw binary) in this buffer
  while(msg_buffer.size() > sizeof(uint32_t)) {
    uint8_t *d = (uint8_t *)(msg_buffer.data());
    uint32_t sz = 0;
    memcpy(&sz, d, sizeof(uint32_t));
    sz = ntohl(sz);
    uint32_t left = msg_buffer.size() - sizeof(uint32_t);
    d += sizeof(uint32_t);
    
    if (sz > left) { //not enought data left
      break;
    }

    //creat a new message which should be deleted after query is sent
    //in callback function
    trace_replay::DNSMsg *msg = new trace_replay::DNSMsg();
    assert(msg);
    msg->ParseFromArray(d, sz);
    
    //reset the dns id
    //set_random_id(qraw->raw);
    //LOG(LOG_DBG, "[%d] set random id [%u]\n", my_pid, get_id(qraw->raw));

    //string raw = msg->raw();
    //print_dns_pkt((uint8_t*)raw.data(), raw.size());
    
    //printf("clientts:%ld.%06ld\n", qraw->ts.tv_sec, qraw->ts.tv_usec);
    //qraw->print();
    //print_dns_pkt(qraw.raw, qraw.len);

    msg_buffer = msg_buffer.substr(sz + sizeof(uint32_t)); //assign msg_buffer for the rest of data 

    struct timeval q_ts = {0, 0};
    q_ts.tv_sec = msg->seconds();
    q_ts.tv_usec = msg->microseconds();

    struct timeval now_ts = {0, 0};
    //evutil_gettimeofday(&now_ts, NULL);

    //check if it is for sync time
    if (msg->sync_time()) { //sync time message from manager
      log_dbg("recv sync_time from manager");
      if (evutil_timerisset(&start_trace_ts))
	log_err("recv sync_time msg but trace start time is set!");
      copy_ts(&start_trace_ts, &q_ts);
      recheck_now_ts(&now_ts);
      copy_ts(&start_real_ts, &now_ts);
      delete msg;
      continue;
    }
    if (!evutil_timerisset(&start_trace_ts))
      log_err("trace start time is NOT set!");

    if (ULLONG_MAX == num_query) { //query count overflow
      LOG(LOG_ERR, "[%d] query count overflow, reset it\n", my_pid);
      num_query = 0;
    }
    
    num_query += 1;
    LOG(LOG_DBG, "[%d] query [%llu] from mananger\n", my_pid, num_query);

    //set a timer event to send the query in the future; we only setup
    //a timer event here, create connection and send will be done when
    //the event is activated
    struct timeval tv = {0, 0};
    struct timeval diff_time = {0, 0};                  //time to trace start time
    struct timeval process_time = {0, 0};               //trace processing time so far
    
    evutil_timersub(&q_ts,   &start_trace_ts, &diff_time);
    recheck_now_ts(&now_ts);
    evutil_timersub(&now_ts, &start_real_ts,  &process_time);

    //not log this for now, since timer might be sensitive
    //LOG(LOG_DBG, "[%d] diff_time %ld.%06ld\n", my_pid, diff_time.tv_sec, diff_time.tv_usec);
    //LOG(LOG_DBG, "[%d] process_time %ld.%06ld\n", my_pid, process_time.tv_sec, process_time.tv_usec);
    
    evutil_timersub(&diff_time, &process_time, &tv);    //the timer time

    //do NOT add shift_ts here since scheduling timer event might be
    //way before the actual query. shift_ts is computed by using the
    //time difference for the actual query. So, adding shift_ts might
    //not be correct, although we need a method to catch up queries.
    //evutil_timeradd(&tv, &shift_ts, &tv);             //time shift; this may not work
    
    if (non_wait || tv.tv_sec < 0) { //send the query immediately
      //tv.tv_sec = 0;
      //tv.tv_usec = 0;
      LOG(LOG_DBG, "[%d] time<0 => send the query[%lld] now\n", my_pid, num_query);
      send_query((void *)msg, num_query);
      num_notimer += 1;
      continue;
    }
    
    cb_arg_t *ctx = new cb_arg_t; //we need to delete this after query is sent in callback function
    ctx->obj = (void *)this;
    ctx->a = (void *)msg;
    ctx->c = num_query;
    
    LOG(LOG_DBG, "[%d] set up a timer event in %ld.%06ld s for query [%llu]\n", my_pid, tv.tv_sec, tv.tv_usec, num_query);

    struct event *ev = evtimer_new(base, &DNSClient::send_query_helper, (void *)ctx);
    ctx->e = (void *)ev;
    assert(ev != NULL);

    //recheck timestamp
    evutil_timerclear(&process_time);
    evutil_timerclear(&tv);
    recheck_now_ts(&now_ts);
    evutil_timersub(&now_ts, &start_real_ts,  &process_time);
    evutil_timersub(&diff_time, &process_time, &tv);
    if (tv.tv_sec < 0) {//send the query immediately
      event_free(ev);
      delete ctx;
      LOG(LOG_DBG, "[%d] time<0 => send the query[%lld] now\n", my_pid, num_query);
      send_query((void *)msg, num_query);
      num_notimer += 1;
      continue;
    }

    if (evtimer_add(ev, &tv) < 0) {
      event_free(ev);
      log_err("fail to add timer event");
    }
    num_timer += 1;
    num_pending_event += 1;
    if (num_pending_event > num_pending_event_max)
      num_pending_event_max = num_pending_event;
  }
}

/*
  helper of send_query
*/
void DNSClient::send_query_helper(evutil_socket_t fd, short which, void *ctx)
{
  assert(which & EV_TIMEOUT);
  cb_arg_t *arg = (cb_arg_t *)ctx;
  struct event *ev = (struct event *)(arg->e);
  event_free(ev); //free this timer event, release memory
  (static_cast<DNSClient *>(arg->obj))->num_pending_event -= 1;
  (static_cast<DNSClient *>(arg->obj))->send_query(arg->a, arg->c);
  delete arg;
}

/*
  send queries via TLS
*/
void DNSClient::send_query_tls(void *arg)
{
  send_query_tcp(arg, true);
}

/*
  tcp send query
*/
void DNSClient::send_query_tcp(void *arg, bool use_tls)
{
  string log_msg = "start to send query via ";
  log_msg += use_tls ? "TLS":"TCP";
  log_dbg(log_msg.c_str());
  
  trace_replay::DNSMsg *msg = (trace_replay::DNSMsg *)arg;
  
  uint16_t raw_len = msg->raw().size();
  uint8_t *raw = new uint8_t[raw_len];
  memcpy(raw, msg->raw().data(), raw_len);
  
  if (raw_len == 0) {
    delete msg;
    if (raw) delete[] raw;
    return;
  }

  string qname = get_query_rr_str(raw, raw_len);
  if (qname.length() == 0) {
    LOG(LOG_ERR, "[%d] [NOQNAME]\n", my_pid);
    //If qname is empty, it is probably because the packet is
    //malformed.  But we should still send it anyway; we do not put it
    //in the record for matching responses
    /*delete[] raw;
    delete msg;
    return;*/
  }
  string key;

  //set rand id at least once in case we get the same key with
  //existing one
  do {
    set_random_id(raw);
    key = to_string(get_id(raw)) + " " + qname;
  } while(query_table.find(key) != query_table.end());

  key = fmt_str(key, " ");

  LOG(LOG_DBG, "[%d] get key: %s\n", my_pid, key.c_str());

  //append the length at the beginning to make a tcp payload
  uint16_t tcp_raw_len = raw_len + sizeof(uint16_t);
  uint8_t *tcp_raw = new uint8_t[tcp_raw_len];
  assert(tcp_raw);
  memset(tcp_raw, 0, tcp_raw_len);
  uint16_t ln = htons(raw_len);
  memcpy(tcp_raw, &ln, sizeof(ln));
  memcpy(tcp_raw + sizeof(ln), raw, raw_len);
  delete[] raw;

  //print_dns_pkt(tcp_raw + 2, tcp_raw_len - 2);

  //get bufferevent for this connection
  string ip = msg->src_ip();
  struct bufferevent *bev = NULL;
  if (src2bev.find(ip) != src2bev.end()) {//found
    bev = src2bev[ip];
    assert(bev != NULL);
    evutil_socket_t fd = bufferevent_getfd(bev);
    LOG(LOG_DBG, "[%d] found fd [%d] for %s\n", my_pid, fd, msg->src_ip().c_str());

    //log query timing
    if (qname.length() != 0 && (output_option & OUTPUT_LATENCY)) {
      struct timeval *qt = new struct timeval;
      evutil_gettimeofday(qt, NULL);
      query_table[key] = qt;
    }
    
    //log timing
    if (output_option & OUTPUT_TIMING)
      record_message_time(tcp_raw+2, tcp_raw_len-2, ip);
    
    if (bufferevent_write(bev, tcp_raw, tcp_raw_len) == -1) {
      log_err("send_query_tcp: bufferevent_write fails");
    }
    delete[] tcp_raw;
    delete msg;
    return;
  }

  //log query timing, we should log the query time HERE since we want
  //to include the tcp handshake time
  if (qname.length() != 0 && (output_option & OUTPUT_LATENCY)) {
    struct timeval *qt = new struct timeval;
    evutil_gettimeofday(qt, NULL);
    query_table[key] = qt;
  }
  //log timing
  if (output_option & OUTPUT_TIMING)
    record_message_time(tcp_raw+2, tcp_raw_len-2, ip);
  
  //not found, create one
  if (use_tls) {
    SSL *ssl = SSL_new(get_ssl_ctx());
    bev = bufferevent_openssl_socket_new(base, -1, ssl, BUFFEREVENT_SSL_CONNECTING,
					 BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
    //bufferevent_openssl_set_allow_dirty_shutdown(bev, 1);
  } else {
    bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);//new bufferevent
  }
  assert(bev);
  src2bev.insert(make_pair(ip, bev)); //insert to map
  bev2src.insert(make_pair(bev, ip)); //insert to map
  server_msg_buffer.insert(make_pair(bev, ""));
  
  if (bufferevent_socket_connect(bev, (struct sockaddr *)&server_addr, sizeof(server_addr))<0) {
    src2bev.erase(ip);
    bev2src.erase(bev);
    server_msg_buffer.erase(bev);
    bufferevent_free(bev);
    log_err("bufferevent_socket_connect fails");
  }

  //nagle
  if (nagle_option == "disable" || nagle_option == "enable") {
    int nagle_flag = (nagle_option == "disable" ? 1 : 0);
    int nagle_fd = bufferevent_getfd(bev);
    if (nagle_fd == -1) {
      log_err("fail to set nagle option for fd[-1]");
    }
    if (setsockopt(nagle_fd, IPPROTO_TCP, TCP_NODELAY, (char *) &nagle_flag, sizeof(int)) < 0) {
      log_err("fail to set nagle option");
    }
    LOG(LOG_DBG, "[%d] %s nagle for fd[%d]\n", my_pid, nagle_option.c_str(), nagle_fd);
  }

  if (time_out > 0) {
    struct timeval read_to = {time_out, 0}; //setup timeout
    LOG(LOG_DBG, "[%d] set read timeout %ld.%06ld\n", my_pid, read_to.tv_sec, read_to.tv_usec);
    bufferevent_set_timeouts(bev, &read_to, NULL);
  } else { //no time out when <= 0
    LOG(LOG_DBG, "[%d] NOT set read timeout\n", my_pid);
  }
  
  bufferevent_setcb(bev, &DNSClient::server_read_cb_helper, NULL, &DNSClient::server_event_cb_helper, this);
  bufferevent_enable(bev, EV_READ|EV_WRITE);

  log_msg = "bufferevent write, send to server via ";
  log_msg += use_tls ? "TLS":"TCP";
  log_dbg(log_msg.c_str());

  //this will work even if before connected
  if (bufferevent_write(bev, tcp_raw, tcp_raw_len) == -1) { //where to bufferevent_write?
    log_err("send_query_tcp_2: bufferevent_write fails");
  }
  delete[] tcp_raw;
  delete msg;
}

/*
  helper for server udp write callback
*/
void DNSClient::server_udp_write_cb_helper(evutil_socket_t fd, short what, void *ctx)
{
  cb_arg_t *arg = (cb_arg_t *)ctx;
  struct event *ev = (struct event *)(arg->e);
  event_free(ev); //free the memory
  assert(what & EV_WRITE);
  (static_cast<DNSClient *>(arg->obj))->server_udp_write_cb(fd, arg->a);
  delete arg;
}

/*
  server udp write callback
*/
void DNSClient::server_udp_write_cb(evutil_socket_t fd, void *ctx)
{
  // cb_arg_t *arg = (cb_arg_t *)ctx;
  // struct event *ev = (struct event *)(arg->e);
  // event_free(ev); //free the memory
  // LOG(LOG_DBG, "[%d] send to server by udp fd [%d]\n", getpid(), fd);
  // assert(what & EV_WRITE);
  LOG(LOG_DBG, "[%d] send to server by udp fd [%d]\n", my_pid, fd);
  trace_replay::DNSMsg *msg = (trace_replay::DNSMsg *)ctx;
  if (output_option & OUTPUT_TIMING)
    record_message_time((uint8_t *)msg->raw().data(), msg->raw().size(), (udpfd2src.count(fd) ? udpfd2src[fd]:"0"));
  if (send(fd, msg->raw().data(), msg->raw().size(), 0) == -1)
    err(1, "send fails");
  delete msg; //query has been set, let's clean data
  //delete arg;
}

/*
  helper for server udp read callback
*/
void DNSClient::server_udp_read_cb_helper(evutil_socket_t fd, short what, void *ctx)
{
  if (what & EV_READ) {
    (static_cast<DNSClient *>(ctx))->server_udp_read_cb(fd);
  } else if (what & EV_TIMEOUT) {
    (static_cast<DNSClient *>(ctx))->server_udp_read_timeout_cb(fd);
  } else {
    LOG(LOG_ERR, "error: !EV_READ&&!EV_TIMEOUT in server_udp_read_cb_helper\n");
  }
}

/*
  server udp read timeout callback
*/
void DNSClient::server_udp_read_timeout_cb(evutil_socket_t fd)
{
  LOG(LOG_DBG, "[%d] server udp fd=%d timeout\n", my_pid, fd);
  if (fd < 0) {
    LOG(LOG_ERR, "[%d] error: server UDP read timeout fd=%d\n", my_pid, fd);
    return;
  }
  if (udp_read_event.find(fd) == udp_read_event.end()) {
    LOG(LOG_ERR, "[%d] error: fd=%d is not in udp_read_event map\n", my_pid, fd);
    return;
  }
  event_free(udp_read_event[fd]);
  close(fd);
  udp_read_event.erase(fd);
  if (udpfd2src.find(fd) == udpfd2src.end()) {
    LOG(LOG_ERR, "[%d] error: fd=%d is not in udpfd2src map", my_pid, fd);
    return;
  }
  LOG(LOG_DBG, "[%d] clean up ip: %s\n", my_pid, udpfd2src[fd].c_str());
  src2udpfd.erase(udpfd2src[fd]);
  udpfd2src.erase(fd);
}

/*
  server udp read callback
*/
void DNSClient::server_udp_read_cb(evutil_socket_t fd)
{
  LOG(LOG_DBG, "[%d] receive from server by udp fd [%d]\n", my_pid, fd);
  uint8_t *buf = new uint8_t[MAX_BUF_SIZE];//to delete after sending
  assert(buf);
  memset(buf, 0, MAX_BUF_SIZE);
  int b = -1;
  b = recv(fd, buf, MAX_BUF_SIZE, 0);
  if (b == -1)
    log_err("recv");

  //xxx: only deal with latency for now
  if (output_option & OUTPUT_LATENCY) {
    sendto_manager(buf, b);
    return;
  }
  if (output_option & OUTPUT_TIMING)
    record_message_time(buf, b, (udpfd2src.count(fd) ? udpfd2src[fd] : "0"));
  delete[] buf;
}

/*
  udp send query
*/
void DNSClient::send_query_udp(void *arg)
{
  log_dbg("start udp send query");
  trace_replay::DNSMsg *msg = (trace_replay::DNSMsg *)arg;

  uint16_t raw_len = msg->raw().size();
  uint8_t *raw = new uint8_t[raw_len];
  memcpy(raw, msg->raw().data(), raw_len);

  if (raw_len == 0) {
    delete msg;
    if (raw) delete[] raw;
    return;
  }

  string qname = get_query_rr_str(raw, raw_len);
  if (qname.length() == 0) {
    LOG(LOG_ERR, "[%d] [NOQNAME]\n", my_pid);
    //if qname empty, it is probably because the packet is
    //malformed. But we should still send it anyway; we do not put it
    //in the record for matching responses.
    /*delete[] raw;
    delete msg;
    return;*/
  }
  string key;

  //set rand id at least once in case we get the same key with
  //existing one
  do {
    set_random_id(raw);
    key = to_string(get_id(raw)) + " " + qname;
  } while(query_table.find(key) != query_table.end());

  key = fmt_str(key, " ");
  //DBG("key: [%s]\n", key.c_str());
  //update raw data since we updated dns id
  msg->set_raw((const char *)raw, (int)raw_len);
  delete[] raw;

  //print_dns_pkt(raw, raw_len);

  //get udp socket
  string ip = msg->src_ip();
  int fd = -1;

  struct event *server_write_ev = NULL;
  struct event *server_read_ev = NULL;

  if (socket_unify & SOCKET_UNIFY_UDP) { //unified udp sockets
    //set fd and later a write event
    fd = unified_udp_fd;
    log_dbg("unified udp sockets: set fd");
  } else if (src2udpfd.find(ip) != src2udpfd.end()) {//not unified and found
    fd = src2udpfd[ip];
    assert(fd != -1);
    LOG(LOG_DBG, "[%d] found udp fd [%d] for %s\n", my_pid, fd, msg->src_ip().c_str());

  } else {//not found, need to create one
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
      log_err("failed to create a new UDP socket");
    if (evutil_make_socket_nonblocking(fd) < 0) {
      evutil_closesocket(fd);
      log_err("evutil_make_socket_nonblocking fails");
    }
    //Does connect on udp socket block? From man connect: If the
    //socket sockfd is of type SOCK_DGRAM, then addr is the address to
    //which datagrams are sent by default, and the only address from
    //which datagrams are received. => no connection -> no block?
    if (connect (fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
      cerr << "error: connect fails:" << errno << endl;
      log_err("failed to connect for new UDP socket");
    }
    src2udpfd.insert(make_pair(ip, fd));
    udpfd2src.insert(make_pair(fd, ip));
    if (num_udp_fd_max < src2udpfd.size()) {
      num_udp_fd_max = src2udpfd.size();
    }
    LOG(LOG_DBG, "[%d] create a new udp fd [%d] for %s\n", my_pid, fd, msg->src_ip().c_str());

    //We made ONE read event for EACH udp socket and it is PERSIST,
    //and add a timeout to close the socket to save memory
    struct timeval read_timeout = {UDP_READ_TIMEOUT, 0};
    server_read_ev = event_new(base, fd, EV_TIMEOUT|EV_READ|EV_PERSIST, &DNSClient::server_udp_read_cb_helper, this);
    assert(server_read_ev != NULL);
    if (event_add(server_read_ev, &read_timeout) < 0){
      //if (event_add(server_read_ev, NULL) < 0){
      event_free(server_read_ev);
      log_err("cannot add server udp read event");
    } else
      LOG(LOG_DBG, "[%d] done add new server udp read event for new fd [%d]\n", my_pid, fd);
    udp_read_event[fd] = server_read_ev;
  }

  //log query timing
  if (qname.length() != 0 && (output_option & OUTPUT_LATENCY)) {
    struct timeval *qt = new struct timeval;
    evutil_gettimeofday(qt, NULL);
    query_table[key] = qt;
  }

  //Need to log time in server_udp_write_cb
  // //log timing
  // if (output_option & OUTPUT_TIMING)
  //   record_message_time((uint8_t *)msg->raw().data(), msg->raw().size());

  //schedule a server write event
  cb_arg_t *ctx = new cb_arg_t; // we need to delete this after query is sent in callback function
  ctx->a = arg;
  ctx->obj = (void *)this;
  server_write_ev = event_new(base, fd, EV_WRITE, &DNSClient::server_udp_write_cb_helper, (void *)ctx);
  ctx->e = (void *)server_write_ev;
  assert(server_write_ev != NULL);
  if (event_add(server_write_ev, NULL) < 0) {
    event_free(server_write_ev);
    log_err("cannot add server udp write event");
  } else
    LOG(LOG_DBG, "[%d] done add new server udp write event for fd [%d]\n", my_pid, fd);
}

/*
  send query
*/
void DNSClient::send_query(void *arg, long long unsigned int ct)
{
  LOG(LOG_DBG, "[%d] start send query [%llu]\n", my_pid, ct);
  
  trace_replay::DNSMsg *msg = (trace_replay::DNSMsg *)arg;

  //deal with time shift
  //this may not work since many event has been setup
  struct timeval current_real_ts, diff_real_ts, diff_trace_ts, q_ts;
  q_ts.tv_sec = msg->seconds();
  q_ts.tv_usec = msg->microseconds();
  evutil_gettimeofday(&current_real_ts, NULL);
  evutil_timersub (&current_real_ts, &start_real_ts,  &diff_real_ts);
  evutil_timersub (&q_ts,            &start_trace_ts, &diff_trace_ts);
  evutil_timersub (&diff_trace_ts,   &diff_real_ts,   &shift_ts);
  LOG(LOG_DBG, "[%d] diff trace: %ld.%06ld\n", my_pid, diff_trace_ts.tv_sec, diff_trace_ts.tv_usec);
  LOG(LOG_DBG, "[%d] diff real: %ld.%06ld\n", my_pid, diff_real_ts.tv_sec, diff_real_ts.tv_usec);
  LOG(LOG_DBG, "[%d] time shift: %ld.%06ld\n", my_pid, shift_ts.tv_sec, shift_ts.tv_usec);

  //which type of socket?
  bool use_tcp = (conn_type == "tcp" || (conn_type == "adaptive" && msg->tcp()));
  bool use_udp = (conn_type == "udp" || (conn_type == "adaptive" && !(msg->tcp())));
  bool use_tls = (conn_type == "tls");

  if (use_tls) {
    send_query_tls(arg);
  } else if (use_tcp) {
    send_query_tcp(arg, false);
  } else if (use_udp) {
    send_query_udp(arg);
  } else {
    log_dbg("cannot send query due to unknown protocol");
  }
}

/*
  send back to manager
*/
void DNSClient::sendto_manager(uint8_t *buf, size_t len)
{  
  //log response timing
  struct timeval rt;
  evutil_gettimeofday(&rt, NULL);

  //get query timing
  string key = to_string(get_id(buf)) + " " + get_query_rr_str(buf, len);
  delete[] buf;
  key = fmt_str(key, " ");
  //DBG("key: [%s]\n", key.c_str());
  if (query_table.find(key) == query_table.end()) {
    LOG(LOG_ERR, "[%d] response for [%s] has no query!\n", my_pid, key.c_str());
    return;
  }
  struct timeval *qt = query_table[key];

  //get latency
  struct timeval latency;
  evutil_timersub(&rt, qt, &latency);
  LOG(LOG_DBG, "[%d] latency %ld.%06ld for %s\n", my_pid, latency.tv_sec, latency.tv_usec, key.c_str());

  //delete from query_table
  query_table.erase(key);
  delete qt;

  //send back to manager
  vector<string> v;
  str_split(key, v);
  bool first = true;
  string tmp = to_string(latency.tv_sec) + " " + to_string(latency.tv_usec);
  for (string e : v) {
    if (first)
      first = false;
    else
      tmp += (" " + e);
  }

  //format string here, manager and commander does not format and just
  //log it
  tmp += "\n";
  int l = tmp.size()+1;
  char *d = new char[l];
  memset(d, 0, l);
  memcpy(d, tmp.c_str(), l-1);
  if (bufferevent_write(manager_bev, d, l) == -1) {
    log_err("sendto_manager: bufferevent_write fails");
  }
  delete[] d;
}

/*
  make a copy of the message and send the timing and id to manager the
  input buf should be raw DNS payload without length field
*/
void DNSClient::record_message_time(uint8_t *data, size_t len, string addr) {
  LOG(LOG_DBG, "[%d] record_message_time: data len = %lu\n", my_pid, len);
  //make a copy of the data
  uint8_t *buf = new uint8_t[len];
  memcpy(buf, data, len);
  
  //get key
  string msg_type = is_query(buf, len , false) ? "Q" : "R";
  string qname = get_query_rr_str(buf, len);
  if (qname.length() == 0) qname = "-";
  string key = msg_type + " " + to_string(get_id(buf)) + " " + qname;
  key = fmt_str(key, " ");
  delete[] buf;

  //log timing
  struct timeval t;
  evutil_gettimeofday(&t, NULL);

  //format string here, manager and commander does not format and just
  //log it
  string tms = to_string(t.tv_usec);
  string tms_full = string(6 - tms.length(), '0') + tms;
  string tmp = addr + " " + to_string(t.tv_sec) + " " + tms_full + " " + key + "\n";
  if (bufferevent_write(manager_bev, tmp.data(), tmp.size()) == -1) {
    log_err("record_message_time: bufferevent_write fails");
  }
  /*int l = tmp.size()+1;
  char *d = new char[l];
  memset(d, 0, l);
  memcpy(d, tmp.c_str(), l-1);
  bufferevent_write(manager_bev, d, l);
  delete[] d;*/
}

/*
  helper for server read callback
*/
void DNSClient::server_read_cb_helper(struct bufferevent *bev, void *ctx)
{
  (static_cast<DNSClient *>(ctx))->server_read_cb(bev);
}

/*
  server read callback (TCP)
*/
void DNSClient::server_read_cb(struct bufferevent *bev)
{
  assert(bev);
  int fd = bufferevent_getfd(bev);
  LOG(LOG_DBG, "[%d] read from server fd [%d]\n", my_pid, fd);

  struct evbuffer *input_buffer = bufferevent_get_input(bev);
  assert(input_buffer);
  size_t len = evbuffer_get_length(input_buffer);
  uint8_t *data = new uint8_t[len];
  assert(data);
  bufferevent_read(bev, data, len);
  // print_dns_pkt(data+2, len-2);

  if (output_option & OUTPUT_TIMING) {
    server_msg_buffer[bev].append(reinterpret_cast<const char*>(data), len);
    while(server_msg_buffer[bev].size() > sizeof(uint16_t)) {
      uint8_t *d = (uint8_t *)(server_msg_buffer[bev].data());
      if (server_msg_buffer[bev].size() < 2) {
	LOG(LOG_ERR, "[%d] error: server_msg_buffer[bev].size() < 2", my_pid);
      }
      uint16_t sz = 0;
      memcpy(&sz, d, sizeof(sz));
      sz = ntohs(sz);
      uint16_t left = server_msg_buffer[bev].size() - sizeof(uint16_t);
      if (sz > left) {
	LOG(LOG_DBG,
	    "[%d] read sz [%d] > left [%d], break! (server might send data and its length separately)\n",
	    my_pid, sz, left);
	break; //not enough data left
      }
      d += sizeof(uint16_t);
      //uint8_t *buf = new uint8_t[sz];//to delete after sending
      //memcpy(buf, d, sz);
      //sendto_manager_time(buf, sz);
      record_message_time(d, sz, (bev2src.count(bev) ? bev2src[bev] : "0"));
      LOG(LOG_DBG, "[%d] trim server message: before[%lu]\n", my_pid, server_msg_buffer[bev].size());
      server_msg_buffer[bev] = server_msg_buffer[bev].substr(sz + sizeof(uint16_t)); //assign msg_buffer for the rest of data
      LOG(LOG_DBG, "[%d] trim server message: after[%lu]\n", my_pid, server_msg_buffer[bev].size());
    }
  }

  if (output_option & OUTPUT_LATENCY) {//xxx: only deal with latency for now
    //send back to manager
    uint8_t *buf = new uint8_t[len-2];//to delete after sending
    memcpy(buf, data+2, len-2);
    sendto_manager(buf, len-2);
  }
  delete[] data;
}

/*
  helper for server event callback
*/
void DNSClient::server_event_cb_helper(struct bufferevent *bev, short which, void *ctx)
{
  (static_cast<DNSClient *>(ctx))->server_event_cb(bev, which);
}

/*
  server event callback
*/
void DNSClient::server_event_cb(struct bufferevent *bev, short which)
{
  evutil_socket_t fd = bufferevent_getfd(bev);
  bool has_err = false;
  //struct event_base *base = bufferevent_get_base(bev);
  //assert(base);
  if (which & BEV_EVENT_CONNECTED) {
    LOG(LOG_DBG, "[%d] fd [%d] connected\n", my_pid, fd);
    //do we do bufferevent write here?
  } else if (which & BEV_EVENT_TIMEOUT) {
    LOG(LOG_DBG, "[%d] fd [%d] timeout\n", my_pid, fd);
    //do we check the read buffer and extend the timeout here?
    has_err = true;
  } else if (which & (BEV_EVENT_EOF|BEV_EVENT_ERROR)) {
    LOG(LOG_DBG, "[%d] fd [%d] %s%s\n", my_pid, fd,
	(which & BEV_EVENT_EOF) ? "got a close":"",
	(which & BEV_EVENT_ERROR) ? evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()):"");
    has_err = true;
  }
  if (has_err) {
    string ip = bev2src[bev];
    LOG(LOG_DBG, "[%d] fd [%d] clean up bev for src %s\n", my_pid, fd, ip.c_str());
    src2bev.erase(ip);
    bev2src.erase(bev);
    server_msg_buffer.erase(bev);
    bufferevent_free(bev);
  }
}

void DNSClient::log_err(const char *s)
{
  err(1, "[%d] [error] %s, abort!", my_pid, s);
}

void DNSClient::log_dbg(const char *s)
{
  LOG(LOG_DBG, "[%d] %s\n", my_pid, s);
}
