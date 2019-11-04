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

#ifndef INPUT_SOURCE_HH
#define INPUT_SOURCE_HH
#include <string>
#include <fstream>
#include "libtrace.h"
#include "dbg.h"
#include "dns_msg.pb.h"
#include <unordered_map>

#define INPUT_SRC_NONE     0x0000U
#define INPUT_SRC_TRACE    0x0001U
#define INPUT_SRC_TEXT     0x0002U
#define INPUT_SRC_RAW      0x0004U

extern std::unordered_map<std::string, unsigned int> input_src_map;

class InputSource {
public:
  InputSource(std::string, std::string);
  ~InputSource();
  trace_replay::DNSMsg *get();

private:
  pid_t my_pid;

  unsigned int input_format = INPUT_SRC_NONE;

  //text file
  std::ifstream ifs;
  bool is_stdin = false;
  bool read_bytes (char *, size_t);
  trace_replay::DNSMsg *get_from_text();
  trace_replay::DNSMsg *process_line(std::string line);
  trace_replay::DNSMsg *get_from_raw();

  //network trace file
  libtrace_t *trace;
  libtrace_packet_t *packet;
  trace_replay::DNSMsg *process_packet();
  trace_replay::DNSMsg *get_from_trace();
  void trace_cleanup();
  void log_dbg(const char *);
};

#endif //INPUT_SOURCE_HH
