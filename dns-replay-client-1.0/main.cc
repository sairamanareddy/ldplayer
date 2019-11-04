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

#include "global_var.h"
#include "utility.hh"
#include "check.hh"
#include "client.hh"
#include "manager.hh"
#include "version.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <thread>
#include <sys/wait.h>
#include <getopt.h>
using namespace std;

int log_level = LOG_INFO;
bool verbose_log = false;

void usage(const char *comm) {
  cerr << " Usage:\n " <<
    comm << " [-i FORMAT:FILE] [-o FORMAT:FILE] [-s IP:PORT] [-r IP:PORT] [-n NUMBER]\n"
    "         [-c TYPE] [-t TIMEOUT] [-l SECONDS] [-p SECONDS]\n"
    "         [-u] [-d] [-f] [-v] [-V] [-h]\n"
    " -i/--input FORMAT:FILE    input stream, required without -d\n"
    "                           format and file separated by colon like FORMAT:PATH\n"
    "                           accepted format: trace, text, raw\n"
    "                           e.g. trace:test.pcap, text:test.fsdb, raw:test.raw\n"
    "                           use '-' as FILE to read from stdin\n"
    " -o/--output FORMAT:FILE   optional output file; accepted format: latency, timing\n"
    "                           latency: output the latency of each query\n"
    "                           timing: output the timing of each query and response\n"
    "                           format and file separated by colon like FORMAT:PATH\n"
    "                           use '-' as FILE to write to stdout\n"
    " -s/--server SERVER        server address and port, separated by colon\n"
    "                           e.g. 1.2.3.4:53\n"
    " -r/--controller ADDRESS   address and port to receive message from controller\n"
    "                           e.g. 5.6.7.8:2018, required in distributed mode (-d)\n"
    " -n/--num-workers NUMBER   number of worker processes\n"
    "                           default is number of CPU cores\n"
    " -c/--connections TYPE     connection type, udp/tcp/tls/adaptive\n"
    "                           'adaptive': query with protocol in input\n"
    "                           NOTE: tls is not implemented yet\n"
    " -t/--timeout TIMEOUT      timeout for tcp/tls connections\n"
    "                           default is 30 seconds, 0 means no timeout\n"
    " -l/--limit SECONDS        preload seconds of trace, used to control memory consumption\n"
    "                           default is none (any negative integer): read all in memory\n"
    "                           option '-f' set this to none automatically\n"
    " -p/--pace SECONDS         set fix pace between queries, e.g. 1, 0.1, 0.05\n"
    "                           equivalent to pace query time in input files\n"
    "                           ignored in distributed mode (-d)\n"
    " -g/--nagle ACTION         'disable' or 'enable' TCP Nagle's algorithm\n"
    " -u/--unify-udp            worker uses one socket for all the UDP queries\n"
    "                           default is to use different sockets for different source IP\n"
    "                           this also disables address to worker mapping\n"
    " -d/--distribute           distributed mode with reading input stream from controller\n"
    " -f/--fast                 send input queries immediately instead of timer\n"
    " -h/--help                 print this message\n"
    " -v/--verbose              verbose log; default is none\n"
    " -V/--version              show the program version\n"
    "\n(" << VERSION << ")\n";
  exit(1);
}

int main(int argc, char *argv[])
{
  const char *comm = argv[0];
  if (argc == 1) usage(comm);

  string server_ip, command_ip;
  string input_file, input_format;
  string output_file, output_format;
  string conn_type = "adaptive", tmp, nagle;

  bool dist = false, non_wait = false;
  int server_port = -1, command_port = -1;
  int trace_limit = -1, time_out = 30;
  int opt = -1, status = 0, *tmp_skt = NULL;
  unsigned int i = 0, num_clients = thread::hardware_concurrency();
  uint32_t socket_unify = SOCKET_UNIFY_NONE, output_option = OUTPUT_NONE;
  pid_t child_pid, wpid, my_pid = getpid();
  double query_pace = -1.0;

  vector<int *> paired_fd; //just keep trace of memory
  vector<int> client_fd, manager_fd, client_pid;

  struct option long_options[] = {
    {"connection",    1, NULL, 'c'},
    {"distribute",    0, NULL, 'd'},
    {"fast",          0, NULL, 'f'},
    {"nagle",         1, NULL, 'g'},
    {"help",          0, NULL, 'h'},
    {"input",         1, NULL, 'i'},
    {"limit",         1, NULL, 'l'},
    {"num-workers",   1, NULL, 'n'},
    {"output",        1, NULL, 'o'},
    {"pace",          1, NULL, 'p'},
    {"controller",    1, NULL, 'r'},
    {"server",        1, NULL, 's'},
    {"timeout",       1, NULL, 't'},
    {"unify-udp",     0, NULL, 'u'},
    {"verbose",       0, NULL, 'v'},
    {"version",       0, NULL, 'V'},
  };

  size_t found;
  while ((opt = getopt_long(argc, argv, "c:g:i:l:n:o:p:r:s:t:dfhuvV", long_options, NULL)) != EOF) {
    tmp.clear();
    switch(opt) {
    case 'c':
      conn_type = optarg;
      conn_type = str_tolower(conn_type);
      break;
    case 'g':
      nagle = optarg;
      nagle = str_tolower(nagle);
      break;
    case 'i':
      tmp = optarg;
      found = tmp.find_first_of(':');
      if (found == string::npos)
	errx(1, "incorrect input format");
      input_format = tmp.substr(0, found);
      input_file = tmp.substr(found + 1);
      break;
    case 'l':
      trace_limit = atoi(optarg);
      break;
    case 'n':
      check_gt0(optarg, "number of workers");
      num_clients = atoi(optarg);
      break;
    case 'o':
      tmp = optarg;
      found = tmp.find_first_of(':');
      if (found == string::npos)
	errx(1, "incorrect output format");
      output_format = tmp.substr(0, found);
      output_file = tmp.substr(found + 1);
      break;
    case 'p':
      tmp = optarg;
      query_pace = stod(tmp);
      break;
    case 'r':
      str_split(optarg, command_ip, tmp, ':');
      check_gt0(tmp, "command port");
      command_port = stoi(tmp);
      break;
    case 's':
      str_split(optarg, server_ip, tmp, ':');
      check_gt0(tmp, "server port");
      server_port = stoi(tmp);
      break;
    case 't':
      check_gt0(optarg, "time out");
      time_out = atoi(optarg);
      break;
    case 'd':
      dist = true;
      break;
    case 'f':
      non_wait = true;
      break;
    case 'u':
      socket_unify |= SOCKET_UNIFY_UDP;
      break;
    case 'v':
      verbose_log = true;
      break;
    case 'V':
      errx(0, VERSION);
      break;
    default:
      usage(comm);
    }
  }

  if (verbose_log) {
    cout << "#trace_limit:" << trace_limit << endl;
  }

  //check input error
  check_set(conn_type, conn_set, "connection type");
  check_str(server_ip, "server ip");
  check_int(server_port, "server port");
  //check_int(time_out, "time out");
  if (time_out < 0) errx(1, "[error] time_out [%d] must be >=0, abort!", time_out);
  if (dist) { //check command address
    if (!input_format.empty() || !input_file.empty())
      warnx("[warn] ignore input file in distributed mode");
    check_str(command_ip, "command ip");
    check_int(command_port, "command port");
  } else {    //check input file
    check_str(input_format, input_file, "input");
    check_map(input_format, input_src_map, "input format");
  }
  check_set(nagle, {"disable", "enable", ""}, "nagle options");

  if (!output_file.empty()) {
    if (output_format == "latency") {
      output_option = OUTPUT_LATENCY;
    } else if (output_format == "timing") {
      output_option = OUTPUT_TIMING;
    } else {
      errx(1, "[error] output format must be either \"latency\" or \"timing\"");
    }
  } else {
    LOG(LOG_WARN, "[warn] no output file; output option is [%u]\n", output_option);
  }
  
  //log debug information
  LOG(LOG_INFO, "# %sdistributed mode\n", dist?"":"none ");
  LOG(LOG_INFO, "# input format: [%s]  path: [%s]\n", input_format.c_str(), input_file.c_str());
  LOG(LOG_INFO, "# output format: [%s] path: [%s]\n", output_format.c_str(), output_file.c_str());
  LOG(LOG_INFO, "# output option: [%u]\n", output_option);
  LOG(LOG_INFO, "# server address: %s  port: %d\n", server_ip.c_str(), server_port);
  LOG(LOG_INFO, "# command address: %s  port: %d\n", command_ip.c_str(), command_port);
  LOG(LOG_INFO, "# number of clients: %d\n", num_clients);
  LOG(LOG_INFO, "# connection: %s\n", conn_type.c_str());
  LOG(LOG_INFO, "# time out: %d\n", time_out);
  LOG(LOG_INFO, "# trace limit: %d seconds\n", trace_limit);
  LOG(LOG_INFO, "# query_pace: %f seconds\n", query_pace);
  LOG(LOG_INFO, "# nagle: %s\n", nagle.c_str());

  LOG(LOG_INFO, "use %s for UDP queries\n", ((socket_unify & SOCKET_UNIFY_UDP) ? "the same socket" : "different sockets"));
  LOG(LOG_INFO, "use %s for TCP queries\n", ((socket_unify & SOCKET_UNIFY_TCP) ? "the same socket" : "different sockets"));
  if (socket_unify & SOCKET_UNIFY_TCP) {
    errx(1, "[error] unified TCP socket reuse has NOT been implemented!");
  }

  /*
   *   +-----fork()---> client processes ---++
   *   |                                    ||
   *  main                                  \/
   * process ----------------------------- wait()
   * (parent)                               /\
   *   |                                    ||
   *   +-----fork()---> manager processs ---++
   */
  
  //set up unix sockets
  LOG(LOG_DBG, "[%d] set up %u pairs of unix sockets for manager-client communication\n", my_pid, num_clients);
  for (i=0; i<num_clients; i++) {
    tmp_skt = new int[2];
    assert(tmp_skt);
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, tmp_skt) == -1)
      err(1, "socketpair");
    paired_fd.push_back(tmp_skt);
    client_fd.push_back(tmp_skt[0]);
    manager_fd.push_back(tmp_skt[1]);
    LOG(LOG_DBG, "[%d] unix socket pair [%d]<->[%d]\n", my_pid, tmp_skt[0], tmp_skt[1]);
  }
    
  //fork sub-client processes
  LOG(LOG_DBG, "[%d] forking %d clients processes\n", my_pid, num_clients);
  for (i=0; i<num_clients; i++) {
    child_pid = fork();
    if (child_pid < 0) {
      err(1, "[error] fork");
    } else if (child_pid == 0) { //child
      my_pid = getpid();
      LOG(LOG_DBG, "[%d] client [%d] is up\n", my_pid, my_pid);
      DNSClient clt(conn_type, nagle, time_out, server_ip, server_port, manager_fd[i],
		    socket_unify, output_option, non_wait);
      clt.start();
      exit(0);
    } else {                     //parent
      LOG(LOG_DBG, "[%d] create client [%d]\n", my_pid, child_pid);
      client_pid.push_back(child_pid);
      continue;                  //nothing else
    }
  }

  //fork manager process
  LOG(LOG_DBG, "[%d] forking 1 manager process\n", my_pid);
  child_pid = fork();
  if (child_pid < 0) {
    err(1, "[error] fork");
  } else if (child_pid == 0) { //child
    my_pid = getpid();
    LOG(LOG_DBG, "[%d] manager [%d] is up\n", my_pid, my_pid);
    Manager mgr(num_clients, dist, conn_type, input_file, input_format,
		output_file, command_ip, command_port, client_fd, client_pid,
		(socket_unify != SOCKET_UNIFY_NONE), trace_limit, query_pace);
    LOG(LOG_DBG, "[%d] sleep for 5s\n", my_pid);
    sleep(5);
    mgr.start();
    exit(0);
  }

  // the main parent process ignores SIGINT due to waiting for child processes
  signal(SIGINT, SIG_IGN);
  
  // the main parent process does wait()
  LOG(LOG_DBG, "[%d] waiting for child processes\n", my_pid);
  while ((wpid = wait(&status)) > 0) {
    LOG(LOG_INFO, "[%d] child process [%d] exits with [%d]\n", my_pid, wpid, status);
    if (WIFEXITED(status)) {
      LOG(LOG_INFO, "[%d] child process [%d] exited, status=%d\n",  my_pid, wpid, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      LOG(LOG_INFO, "[%d] child process [%d] killed by signal %d\n",  my_pid, wpid, WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
      LOG(LOG_INFO, "[%d] child process [%d] stopped by signal %d\n",  my_pid, wpid, WSTOPSIG(status));
    } else if (WIFCONTINUED(status)) {
      LOG(LOG_INFO, "[%d] child process [%d] continued\n", my_pid, wpid);
    }
  }

  //clean up unix socket: underline manager and client processes
  //should close these sockets, not the main function
  LOG(LOG_DBG, "[%d] clean up unix sockets\n", my_pid);
  for (int *skt : paired_fd)
    delete[] skt;
  
  LOG(LOG_INFO, "[%d] ends\n", my_pid);
  return 0;
}
