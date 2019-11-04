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

#include <iostream>
#include <string>
#include <unordered_set>
#include "postman.h"
#include "reader.h"
#include "utility.hh"
#include "filter.h"
#include "global_var.h"
#include <sys/wait.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
using namespace std;

int log_level = LOG_ERR;

const unordered_set<string> trace_format({"trace", "text", "raw"});

static void sig_handler (int sig)
{
  cerr << "["<< getpid()<<"]"<< ": receive signal: " << sig << endl;
}

static bool ValidateInput(const char *flagname, const string &input) {
  auto found = input.find_first_of(':');
  if (found == string::npos || !trace_format.count(input.substr(0, found))) {
    return false;
  }
  return true;
}

static bool ValidateStringNotEmpty(const char *flagname, const string &s) {
  if (s.empty()) return false;
  return true;
}

static bool ValidatePort(const char *flagname, int32_t port) {
   if (port < 1 || port > 65535) return false;
   return true;
}

static bool ValidateNumClients(const char *flagname, int32_t num_clients) {
   if (num_clients < 1) return false;
   return true;
}

DEFINE_string(input, "",
	      "input format and path separated by colon."
	      "e.g. trace:PATH, text:PATH, raw:PATH");
DEFINE_validator(input, &ValidateInput);
DEFINE_string(output, "", "output, file or '-' for stdout");
DEFINE_string(address, "", "listening address");
DEFINE_validator(address, &ValidateStringNotEmpty);
DEFINE_string(filter, "",
	      "colon separated numbers to indicate different portions of queries to different clients."
	      "e.g. string \"10,30,60\" means 10%, 30% and 60% queries to 1st, 2nd and 3rd clients."
	      "or a file that provides such information.");
DEFINE_int32(port, -1, "listening port");
DEFINE_validator(port, &ValidatePort);
DEFINE_int32(num_clients, -1, "listening port");
DEFINE_int32(trace_limit, -1,
	     "preload seconds of trace, used to control memory consumption."
	     "default is none (0 or negative integer): read all in memory");
DEFINE_validator(num_clients, &ValidateNumClients);
DEFINE_bool(dry_run, false,
	    "start controller without accepting connections. used to debug");

int main(int argc, char *argv[])
{
  string usage("Sample usage:\n");
  usage += argv[0];
  usage += " --input raw:test --address=127.0.0.1 --port=10053 --num_clients=20 --dry_run";

  FLAGS_logtostderr = 1;
  google::InitGoogleLogging(argv[0]);
  gflags::SetUsageMessage(usage);
  gflags::SetVersionString("1.0");
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  auto colon = FLAGS_input.find_first_of(':');
  string input_format = FLAGS_input.substr(0, colon);
  string input_file = FLAGS_input.substr(colon + 1);
  int status = 0, skt[2] = {-1, -1};
  pid_t child_pid, wpid, my_pid = getpid();

  if (FLAGS_dry_run) {
    FLAGS_v = 100;
  }
  VLOG(1) << "# dry_run: " << (FLAGS_dry_run ? "enable" : "disable");
  VLOG(1) << "# input: " << FLAGS_input;
  VLOG(1) << "#   input_format: " << input_format;
  VLOG(1) << "#   input_file:   " << input_file;
  VLOG(1) << "# output: " << FLAGS_output;
  VLOG(1) << "# num_clients: " << FLAGS_num_clients;
  VLOG(1) << "# address: " << FLAGS_address << ":" << FLAGS_port;
  VLOG(1) << "# trace_limit: " << FLAGS_trace_limit;
  VLOG(1) << "# filter: " << FLAGS_filter;

  Filter client_filter;
  if (!FLAGS_filter.empty()) {
    client_filter.Set(FLAGS_filter, FLAGS_num_clients);
    if (!client_filter.Valid()) {
      client_filter.Print();
      LOG(ERROR) << "input filter is invalid";
      exit(1);
    }
    if (FLAGS_dry_run) {
      client_filter.Print();
    }
  }

  //main process *reader*: read the input file and writes to unix socket
  //child process *postman*: one event base to handle *reader* and clients' sockets
  if (!FLAGS_dry_run) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, skt) == -1) {
      err(1, "socketpair");
    }
    if (skt[0] < 0 || skt[1] < 0) {
      err(1, "socketpair fails");
    }
  }
  VLOG(2) << "[" << my_pid << "] unix socket pair [" << skt[0] << "]<->[" << skt[1] << "]";

  child_pid = fork();
  if (child_pid < 0) {
    err(1, "[error] fork");
  } else if (child_pid == 0) {//child process: postman
    my_pid = getpid();
    VLOG(2) << "[" << my_pid << "] postman [" << my_pid << "] is up";
    if (!FLAGS_dry_run) {
      Postman cmd (FLAGS_output, FLAGS_address, FLAGS_port, FLAGS_num_clients, skt[1], client_filter);
      cmd.start();
    }
    VLOG(2) << "[" << my_pid << "] ends";
    return 0;
  }

  //parent process: reader
  VLOG(2) << "[" << my_pid << "] create postman [" << child_pid << "]";
  VLOG(2) << "[" << my_pid << "] wait for 3 seconds";
  sleep(3);

  if (!FLAGS_dry_run) {
    Reader rd (input_file, input_format, skt[0]);
    if (rd.unblock(POSTMAN_FLAG)) {
      rd.read_input(FLAGS_trace_limit);
    }
  }
  
  signal (SIGINT, sig_handler);
  signal (SIGTERM, sig_handler);
  VLOG(2) << "[" << my_pid << "] waiting for postman processes";
  while ((wpid = wait(&status)) > 0) {
    VLOG(2) << "[" << my_pid << "] postman [" << wpid << "] exits with [" << status << "]";
  }

  VLOG(2) << "[" << my_pid << "] ends";
  return 0;
}
