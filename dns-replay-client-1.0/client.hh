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

#ifndef CLIENT_HH
#define CLIENT_HH

#include "global_var.h"
#include <string>
#include <set>
#include <event2/event.h>
#include <unordered_map>
#include <netinet/in.h>

#include <openssl/ssl.h>

#define SOCKET_UNIFY_NONE  0x0000U
#define SOCKET_UNIFY_UDP   0x0001U
#define SOCKET_UNIFY_TCP   0x0002U

#define OUTPUT_NONE        0x0000U
#define OUTPUT_LATENCY     0x0001U
#define OUTPUT_TIMING      0x0002U
#define OUTPUT_ALL         0xFFFFU

const std::set<std::string> conn_set = {"udp", "tcp", "tls", "adaptive"};

class DNSClient{

public:
  DNSClient(std::string, std::string, int, std::string, int, int, uint32_t, uint32_t, bool);
  ~DNSClient();
  void start();
  struct event_base *get_base();
  long long unsigned int get_pending_event_max();
  long long unsigned int get_num_timer();
  long long unsigned int get_num_notimer();

private:
  pid_t my_pid;
  int server_port = -1;
  int time_out;
  int manager_fd = -1;
  bool non_wait = false;
  long long unsigned int num_query; // this is used for debug
  long long unsigned int num_pending_event = 0;
  long long unsigned int num_pending_event_max = 0;
  long long unsigned int num_timer = 0;
  long long unsigned int num_notimer = 0;
  long long unsigned int num_udp_fd_max = 0;

  uint32_t socket_unify = SOCKET_UNIFY_NONE;
  uint32_t output_option = OUTPUT_NONE;
  int unified_udp_fd = -1;
  struct event *unified_udp_read_event = NULL;

  struct timeval start_trace_ts = {0, 0};
  struct timeval start_real_ts = {0, 0};
  struct timeval shift_ts = {0, 0};

  struct sockaddr_in server_addr;
  
  std::string conn_type;
  std::string nagle_option;
  std::string server_ip;
  std::string msg_buffer;

  struct event_base *base;
  struct bufferevent *manager_bev;

  std::unordered_map<std::string, int> src2udpfd;                //index by src ip and udp fd
  std::unordered_map<int, std::string> udpfd2src;                //index by udp fd and src ip
  std::unordered_map<int, struct event *> udp_read_event;        //index by udp fd and server udp read event
  std::unordered_map<std::string, struct bufferevent *> src2bev; //index by src ip and struct bufferevent *
  std::unordered_map<struct bufferevent *, std::string> bev2src; //index by struct bufferevent * and src_ip
  std::unordered_map<std::string, struct timeval *> query_table; //index by (dns-id + qname) and query time
  std::unordered_map<struct bufferevent *, std::string> server_msg_buffer; //server tcp message buffer for each bev

  SSL_CTX *ssl_ctx {nullptr};
  SSL_CTX *get_ssl_ctx();
  void init_ssl();

  void sendto_manager(uint8_t *, size_t);
  void record_message_time(uint8_t *, size_t, std::string);
  
  static void send_query_helper(evutil_socket_t, short, void *);
  void send_query(void *, long long unsigned int);
  void send_query_udp(void *);
  void send_query_tcp(void *, bool);
  void send_query_tls(void *);
  
  static void manager_read_cb_helper(struct bufferevent *, void *);
  void manager_read_cb(struct bufferevent *);

  static void manager_event_cb_helper(struct bufferevent *, short, void *);
  void manager_event_cb(struct bufferevent *, short);

  static void server_udp_read_cb_helper(evutil_socket_t, short, void *);
  void server_udp_read_cb(evutil_socket_t);
  void server_udp_read_timeout_cb(evutil_socket_t);

  static void server_udp_write_cb_helper(evutil_socket_t, short, void *);
  void server_udp_write_cb(evutil_socket_t, void *);
  
  static void server_read_cb_helper(struct bufferevent *, void *);
  void server_read_cb(struct bufferevent *);

  static void server_event_cb_helper(struct bufferevent *, short, void *);
  void server_event_cb(struct bufferevent *, short);
  
  void log_err(const char *);
  void log_dbg(const char *);
};

#endif //CLIENT_HH
