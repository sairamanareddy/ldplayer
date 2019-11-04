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

#ifndef MANAGER_HH
#define MANAGER_HH

#include "global_var.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include "libtrace.h"
#include "input_source.hh"
//#include <netinet/in.h>

struct client_t {
  uint8_t idx;
  int fd;
  pid_t pid;
  struct bufferevent *bev;
};

class Manager{
public:
  Manager(int, bool, std::string,
	  std::string, std::string,
	  std::string,
	  std::string, int,
	  std::vector<int>, std::vector<int>,
	  bool, int, double);
  ~Manager();
  void start();

private:
  pid_t my_pid;
  bool dist;
  bool done_sync_time;
  bool disable_mapping = false;
  int num_clients;
  int command_port;
  int com_fail_retry = 0;
  double query_pace = -1.0;
  double query_pace_ts = -1.0;
  double trace_limit = -1.0;

  InputSource *ins = NULL;
  
  std::string input_file;
  std::string input_format;
  std::string output_file;
  std::string command_ip;
  std::string conn_type;
  std::string com_msg_buffer;

  std::vector<int> client_fd;
  std::vector<int> client_pid;
  std::vector<client_t *> client_vec;
  std::unordered_map<int, int> client_idx2fd;    //index by (idx, fd)
  std::unordered_map<int, int> client_fd2pid;    //index by (fd, pid)
  std::unordered_map<std::string, int> client_src2fd; //index by (src_ip, pid)

  //output file
  std::ofstream out_fs;

  struct sockaddr_in com_addr;
  struct bufferevent *com_bev = NULL;
  struct event_base *evbase = NULL;

  void read_input_file();
  void main_event_loop();

  int rand_client_fd(char *);

  static void read_client_cb_helper(struct bufferevent *, void *);
  void read_client_cb(struct bufferevent *);

  static void com_read_cb_helper(struct bufferevent *, void *);
  void com_read_cb(struct bufferevent *);

  static void com_bevent_cb_helper(struct bufferevent *, short, void *);
  void com_bevent_cb();
  
  void clean_up();
  void terminate_clients();
  
  void log_dbg(const char *);
  void log(int, const char *);
  void log_err(const char *);
  void log_errx(const char *);
  void log_warn(const char *);
  void log_warnx(const char *);
};

#endif //MANAGER_HH
