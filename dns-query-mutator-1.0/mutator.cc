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

#include "mutator.hh"
#include <iostream>
#include <thread>
#include <cassert>

#include "global_var.h"
#include "str_util.hh"
#include "dns_util.hh"
#include "file_util.hh"
#include <ldns/ldns.h>
#include <err.h>
#include <time.h>
using namespace std;

#define OUTPUT_FORMAT_NONE   0x00000000u
#define OUTPUT_FORMAT_TEXT   0x00000001u
#define OUTPUT_FORMAT_RAW    0x00000002u
#define OUTPUT_FORMAT_TRACE  0x00000004u

unordered_map<string, unsigned int> OUTPUT_FORMAT_MAP = {
  {"text",  OUTPUT_FORMAT_TEXT},
  {"raw",   OUTPUT_FORMAT_RAW}
};

bool valid_option (unsigned int a, unsigned int down, unsigned int up) {
  return (a >= down && a <= up && (a & (a-1)) == 0);
}

bool valid_dns_option (unsigned int p) {
  return valid_option (p, DNS_OPCODE, EDNS_Z);
}

bool valid_output_format (unsigned int p) {
  return valid_option (p, OUTPUT_FORMAT_TEXT, OUTPUT_FORMAT_RAW);
}

Mutator::Mutator(string ifn, string ift, string ofn, string oft, bool f, unordered_map<unsigned int, string> &m)
{
  unordered_map<unsigned int, pair<int, uint32_t>> tmp;
  for (auto it : m) {
    if (!valid_dns_option(it.first))
      errx(1, "invalid dns option [%u]", it.first);
    string s1, s2;
    str_split(it.second, s1, s2, ':');
    if (!is_number(s1, true))
      errx(1, "Mutator::Mutator(): [%s] is not positive number", s1.c_str());
    if (!is_number(s2, true))
      errx(1, "Mutator::Mutator(): [%s] is not positive number", s2.c_str());
    tmp[it.first] = make_pair(stoi(s1), (uint32_t)stoi(s2));
  }
  init(ifn, ift, ofn, oft, f, tmp);
}

Mutator::Mutator(string ifn, string ift, string ofn, string oft, bool f, unordered_map<unsigned int, pair<int, uint32_t>> &m)
{
  init(ifn, ift, ofn, oft, f, m);
}

void Mutator::init(string ifn, string ift, string ofn, string oft, bool f, unordered_map<unsigned int, pair<int, uint32_t>> &m)
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  srand (time(NULL));

  input_file = ifn;
  input_format = ift;
  
  output_file = ofn;
  output_format = OUTPUT_FORMAT_MAP[oft];
  if (!valid_output_format (output_format))
    errx(1,"unknown output format");
  is_stdout = (output_file == "-");

  //fsdb header
  string fsdb_header = "#fsdb -F t time srcip protocol";
  for (string s : DNS_MSG_HEADER)
    fsdb_header += " " + s;
  fsdb_header += "\n";
  
  if (!is_stdout) {
    if (output_format & OUTPUT_FORMAT_RAW)
      ofs.open(output_file, ofstream::out | ofstream::binary);
    else if (output_format & OUTPUT_FORMAT_TEXT)
      ofs.open(output_file);
    else
      errx(1, "unknown output format: %u", output_format);
    if (!ofs.is_open())
      err(1, "cannot open output file");
  }

  if (output_format & OUTPUT_FORMAT_TEXT) {
    if (ofs.is_open())
      ofs << fsdb_header;
    else
      cout << fsdb_header;
  }
  
  infinite = f;
  opt = m; //copy

  if (ifn != "-" && !check_path(input_file.c_str(), false))
    errx(1, "input file is invalid!");

  ins = new InputStream(ifn, ift, verbose_log);
  assert(ins != NULL);
}

Mutator::~Mutator()
{
  if (ins != NULL)
    delete ins;
  if (!is_stdout && ofs.is_open())
    ofs.close();
  for (trace_replay::DNSMsg *msg : warehouse) {
    delete msg;
  }
}

void Mutator::start()
{
  assert(ins != NULL);
  trace_replay::DNSMsg *msg = NULL;
  string raw;
  while((msg = ins->get()) != NULL) {
    assert(msg);
    raw = msg->raw();
    if (raw.size() == 0) {//not a valid query                                        
      delete msg;
      msg = NULL;
      continue;
    }
    if (!opt.empty()) {
      mutate_query(msg);
      if (msg->raw().size() == 0) {
	warnx("change queries does not work!");
	delete msg;
	msg = NULL;
	continue;
      }
    }

    if (infinite) {
      warehouse.push_back(msg);
    } else {
      write_output(msg);
      delete msg;
    }
  }

  if (infinite) {
    for (unsigned int i=0; i<warehouse.size(); i++) {
      write_output(warehouse[i]);
      if (i == (warehouse.size()-1)) i=-1;
    }
  }
}

void Mutator::mutate_query(trace_replay::DNSMsg *msg)
{
  uint8_t *raw = NULL;
  size_t len = 0;
  string raw_old = msg->raw();
  msg->clear_raw();

  unordered_map<uint32_t, uint32_t> opt_applied;
  for (auto o : opt) {
    uint32_t part = o.first;
    uint32_t val = o.second.second;
    int prob = o.second.first;
    bool do_change = (rand() % 100 + 1 <= prob);
      
    // for 1 bit: prob => val, (1-prob => ~val)
    // for multi-bit: prob => val, set only when in prob
    // 1-bit: aa, tc, rd, ra, z, ad, cd, edns-do
    // multi-bit: opcode, edns-udp-size, edns-extended-code, edns-version, edns-z
    if (part & DNS_AA || part & DNS_TC || part & DNS_RD || part & DNS_RA || part & DNS_Z ||
	part & DNS_AD || part & DNS_CD || part & EDNS_DO) {
      if (do_change) {
	opt_applied[part] = (val > 0 ? 1 : 0);
      } else {
	opt_applied[part] = (val > 0 ? 0 : 1);
      }
    } else if (part & DNS_OPCODE || part & EDNS_UDP_SIZE || part & EDNS_EXTENDED_RCODE ||
	       part & EDNS_VERSION || part & EDNS_Z) {
      if (do_change)
	opt_applied[part] = val;
    }
  }

  if (!change_dns_pkt(&raw, &len, (uint8_t*)raw_old.data(), raw_old.size(), opt_applied))
    warnx("failed to change queries!");
    
  msg->set_raw((const char*)raw, (int)len);
  LDNS_FREE(raw);
  
  
  /*  string raw = msg->raw();
  print_dns_pkt((uint8_t*)raw.data(), raw.size());*/
}

void write_data (void *p, size_t n, ofstream &ofs)
{
  size_t d = 0;
  if (ofs.is_open()) {//write to file                                                
    ofs.write((const char *)p, n);
  } else {//write to stdout                                                          
    d = fwrite(p, 1, n, stdout);
    if (d != n)
      err(1, "fail to write data to stdout d[%lu] != n[%lu]",d,n);
  }
}

void write_raw (trace_replay::DNSMsg *msg, ofstream &ofs)
{
  string msg_str;
  if (!(msg->SerializeToString(&msg_str)))
    err(1, "serialize message fails");

  uint32_t sz = msg_str.size();
  sz = htonl(sz);

  //length                                                                           
  write_data((void *)&sz, sizeof(sz), ofs);

  //data                                                                             
  write_data((void *)msg_str.data(), msg_str.size(), ofs);
}

void write_text(trace_replay::DNSMsg *msg, ofstream &ofs)
{
  if (!msg) errx(1, "msg is NULL");
  string raw = msg->raw();
  string l;
  l += to_string(msg->seconds()) + ".";
  string usec = to_string(msg->microseconds());
  for (unsigned int i=0; i < 6 - usec.length(); i++)
    l += "0";
  l += usec + "\t";
  l += msg->src_ip() + "\t";
  l += (msg->tcp()?"tcp\t":"udp\t");
  string msg_str;
  if (!dns_msg_str((uint8_t*)raw.data(), raw.size(), msg_str)) {
    warnx("cannot get dns msg sring: %s", l.c_str());
    return;
  }
  if (ofs.is_open())
    ofs << l << msg_str << endl;
  else
    cout << l << msg_str << endl;
}

void Mutator::write_output(trace_replay::DNSMsg *msg)
{
  switch(output_format){
  case OUTPUT_FORMAT_RAW:
    write_raw(msg, ofs);
    break;
  case OUTPUT_FORMAT_TEXT:
    write_text(msg, ofs);
    break;
  default:
    errx(1, "unknown output format!");
  }
}
