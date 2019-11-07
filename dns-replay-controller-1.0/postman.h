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

#ifndef POSTMAN_HH
#define POSTMAN_HH

#include <string>
#include <vector>
#include <random>
#include <fstream>
#include <event2/event.h>
#include <unordered_map>
#include <netinet/in.h>

#include "filter.h"

// postman reads input from the reader by socket; it then distributes
// the query data to different clients based on client filter or
// randomly

class Postman{
public:
  Postman(std::string, std::string, int, int, int, Filter &);
  ~Postman();
  void start();

private:
  pid_t my_pid;
  int listen_port;
  int skt;
  unsigned int num_clients;
  bool start_read_input = false;
  bool done_sync_time = false;

  Filter client_filter;
  
  struct addrinfo* listen_addr;
  struct event_base *base = NULL;
  struct bufferevent *skt_bev;
  
  std::string skt_buffer;
  std::string listen_ip;
  std::string output_file;
  
  std::ofstream out_fs;

  std::unordered_map<std::string, int> client_src2fd;          //index by (src_ip, fd)
  std::unordered_map<int, struct bufferevent *> client_fd2bev; //index by (fd, bev)
  std::unordered_map<int, std::vector<std::string> > client_fd2src; //index by (fd, vector of src_ip)

  std::vector<int> client_fds;   //store a list of client fd
  
  std::mt19937 mt_generator;
  std::uniform_int_distribution<int> uniform_client;
  std::uniform_int_distribution<int> uniform_filter;

  struct bufferevent *rand_client(const char *);
  
  static void accept_conn_cb_helper(struct evconnlistener *, evutil_socket_t,
				    struct sockaddr *, int, void *);
  void accept_conn_cb(struct evconnlistener *, evutil_socket_t,
		      struct sockaddr *, int);

  static void client_read_cb_helper(struct bufferevent *, void *);
  void client_read_cb(struct bufferevent *);

  static void client_bevent_cb_helper(struct bufferevent *, short, void *);
  void client_bevent_cb(int);

  static void skt_read_cb_helper(struct bufferevent *, void *);
  void skt_read_cb(struct bufferevent *);

  static void skt_event_cb_helper(struct bufferevent *, short, void *);
  void skt_event_cb(struct bufferevent *, short);
};

#endif //POSTMAN_HH
