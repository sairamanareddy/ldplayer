/*
 * Copyright (C) 2017-2018 by the University of Southern California
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

/*
  read input stream (file or stdin) in different format
  change packet data when specified
  output dns_msg
  this class is mostly the same as that in input_source.hh
  but with minor function change
*/

#ifndef INPUT_STREAM_HH
#define INPUT_STREAM_HH

#include "dns_msg.pb.h"
#include <string>
#include <fstream>
#include "libtrace.h"
#include <unordered_map>

#define INPUT_STREAM_NONE   0x00000000u
#define INPUT_STREAM_TRACE  0x00000001u
#define INPUT_STREAM_TEXT   0x00000002u
#define INPUT_STREAM_RAW    0x00000004u

class InputStream {

public:
  InputStream(std::string, std::string);
  InputStream(std::string, std::string, bool);
  ~InputStream();
  trace_replay::DNSMsg *get();
  
private:
  bool verbose;
  bool is_stdin;    
  unsigned int input_format;
  std::ifstream ifs;

  void init(std::string, std::string, bool);
  
  //changeable options
  /*std::unordered_map<std::string, std::pair<int, uint32_t>> opt;*/
  
  //text file
  bool read_bytes (char *, size_t);
  trace_replay::DNSMsg *get_from_text();
  trace_replay::DNSMsg *process_line(std::string line);

  //raw
  trace_replay::DNSMsg *get_from_raw();

  //network trace file
  libtrace_t *trace;
  libtrace_packet_t *packet;
  trace_replay::DNSMsg *process_packet();
  trace_replay::DNSMsg *get_from_trace();
  void trace_cleanup();

  void log_dbg(const char *);
};

#endif //INPUT_STREAM_HH
