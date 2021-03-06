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

#include "input_source.hh"
#include "global_var.h"
#include "dns_util.hh"
#include <ldns/ldns.h>
#include <iostream>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <vector>
#include "utility.hh"
using namespace std;

#define BUF_SIZE  4096

unordered_map<string, unsigned int> input_src_map = {
  {"text",  INPUT_SRC_TEXT},
  {"trace", INPUT_SRC_TRACE},
  {"raw",   INPUT_SRC_RAW}
};

InputSource::InputSource(string fn, string ft)
{
  my_pid = getpid();
  input_format = input_src_map[ft];
  is_stdin = (fn == "-");
  
  if (input_format & INPUT_SRC_TRACE) {//read trace file; stdin(-) is ok
    trace = trace_create(fn.c_str());
    packet = trace_create_packet();
    if (!packet) {
      trace_cleanup();
      err(1, "cannot create libtrace packet");
    }
    if (trace_is_err(trace)) {
      trace_perror(trace, "opening trace file");
      trace_cleanup();
      err(1, "cannot open trace file");
    }
    if (trace_start(trace) == -1) {
      trace_perror(trace, "starting trace");
      trace_cleanup();
      err(1, "cannot start trace");
    }
  } else if (!is_stdin) { //not read from stdin: open input file
    if (input_format & INPUT_SRC_TEXT) {
      ifs.open(fn);
    } else if (input_format & INPUT_SRC_RAW) {
      ifs.open(fn, ifstream::in | ifstream::binary);
    }
    if (!ifs.is_open())
      err(1, "cannot open input file");
  }
}

InputSource::~InputSource()
{
  if (input_format & INPUT_SRC_TRACE)
    trace_cleanup();
  else if (!is_stdin)
    ifs.close();
}

void InputSource::trace_cleanup()
{
  log_dbg("clean up trace");
  if (trace) {
    trace_destroy(trace);
    trace = NULL;
  }

  if (packet) {
    trace_destroy_packet(packet);
    packet = NULL;
  }
}

/*
  get one trace_replay::DNSMsg from input file; return
  trace_replay::DNSMsg without raw data for invalid query pakcet,
  caller should ignore it and continue; return NULL for end of file,
  caller should exit and not call get again
*/
trace_replay::DNSMsg *InputSource::get()
{
  switch(input_format){
  case INPUT_SRC_TRACE:
    return get_from_trace();
    break;
  case INPUT_SRC_TEXT:
    return get_from_text();
    break;
  case INPUT_SRC_RAW:
    return get_from_raw();
    break;
  default:
    warnx("unknown input type!");
    return NULL;
  }
}

bool InputSource::read_bytes (char *buf, size_t n)
{
  if (is_stdin) {
    if (n != fread((void *)buf, 1, n, stdin)) {
      warn("[warn] fail to read stdin");
      return false;
    }
  } else {
    if (!ifs.read(buf, n)) {
      warn("[warn] fail to read file");
      return false;
    }
  }
  return true;
}

trace_replay::DNSMsg *InputSource::get_from_raw()
{
  uint32_t sz = 0;
  char buf[BUF_SIZE];
  while(true) {
    sz = 0;
    if (!read_bytes((char *)&sz, sizeof(sz)))
      break;

    sz = ntohl(sz);
    
    memset(buf, 0, sizeof(buf));
    if (!read_bytes(buf, sz))
      break;

    trace_replay::DNSMsg *msg = new trace_replay::DNSMsg();
    assert(msg);
    msg->ParseFromArray(buf, sz);
    return msg;
  }
  return NULL;
}

trace_replay::DNSMsg *InputSource::get_from_text()
{
  string line;
  do {
    line.clear();
    if (!getline((is_stdin ? (cin):(ifs)), line))
      return NULL;
  } while (line.length() == 0 || line == "\n" || line.at(0) == '#');

  return process_line(line);
}

trace_replay::DNSMsg *InputSource::process_line(string line)
{
  vector<string> v;
  trace_replay::DNSMsg *msg = new trace_replay::DNSMsg();
  assert(msg);

  //this should not happen but just in case
  if (line.length() == 0 || line == "\n" || line.at(0) == '#')
    return msg;
  
  str_split(line, v);
  if (v.size() != 6) {//input: time, ip, qname, qclass, qtype, protocol
    warnx("input line is invalid: %s", line.c_str());
    return msg;
  }
  query_t *q = new query_t;
  assert(q);
  //build dns query
  uint8_t *qraw = NULL;
  size_t qlen = 0;
  q->qname  = v[2].c_str();
  q->qclass = v[3].c_str();
  q->qtype  = v[4].c_str();

  if (strcmp(q->qname, "-") == 0 &&
      strcmp(q->qclass, "-") == 0 &&
      strcmp(q->qtype, "-") == 0) { //this is an empty query
    build_dns_pkt(&qraw, &qlen);
  } else {
    build_dns_query(&qraw, &qlen, q);
  }
  msg->set_raw((const char *)qraw, (int)qlen);
  LDNS_FREE(qraw);
  delete q;

  //src address
  msg->set_src_ip(v[1]);

  //time stamp
  msg->set_seconds(0);
  msg->set_microseconds(0);
  int dot_c = count(v[0].begin(), v[0].end(), '.');
  if (dot_c > 1 || dot_c < 0)
    errx(1, "[error] wrong timestamp format: %s", v[0].c_str());
  else if (dot_c == 0)
    msg->set_seconds(stoi(v[0]));
  else { //ok, only one dot
    string t1, t2;
    str_split(v[0], t1, t2, '.');
    if (!t1.empty())
      msg->set_seconds(stoi(t1));
    if (!t2.empty())
      msg->set_microseconds(stoi(t2));
  }
  /*double ts = stod(v[0]);
  msg->set_seconds((int)ts);
  msg->set_microseconds((ts - msg->seconds()) * 1000000);*/

  //other information
  msg->set_tcp(v[5] == "tcp");
  msg->set_ipv4(true);//v4 only for now
  
  return msg;
}

/*
  get a trace_replay::DNSMsg from trace file
*/
trace_replay::DNSMsg *InputSource::get_from_trace()
{
  if (trace_read_packet(trace, packet) > 0)
    return process_packet();
  else {//something went wrong or end of file
    if (trace_is_err(trace)) {
      trace_perror(trace, "reading packets");
      trace_cleanup();
      err(1, "error in reading packets");
    }
    return NULL;
  }
}

/*
  process each packet from trace; return trace_replay::DNSMsg without raw data for
  invalid query packet;
*/
trace_replay::DNSMsg *InputSource::process_packet()
{
  libtrace_udp_t *udp = NULL;
  libtrace_tcp_t *tcp = NULL;
  libtrace_ip6_t *ip6 = NULL;
  libtrace_ip_t  *ip  = NULL;
  void *cp            = NULL;
  uint8_t proto       = 0;
  uint16_t ethertype  = 0;
  uint32_t left       = 0;

  trace_replay::DNSMsg *msg = new trace_replay::DNSMsg();
  assert(msg);

  cp = trace_get_layer3(packet, &ethertype, &left);

  if (cp == NULL || left == 0) {
    log_dbg("no payload left");
    return msg;
  }

  uint16_t src_port = 0, dst_port = 0;
  dst_port = trace_get_destination_port(packet);
  src_port = trace_get_source_port(packet);
  if (dst_port != 53 && src_port != 53) {
    log_dbg("port is not 53");
    return msg;
  }

  int addr_len = INET_ADDRSTRLEN;
  if (ethertype == TRACE_ETHERTYPE_IP) {
    ip = (libtrace_ip_t *)cp;
    cp = trace_get_payload_from_ip(ip, &proto, &left);
  } else if (ethertype == TRACE_ETHERTYPE_IPV6) {
    ip6 = (libtrace_ip6_t *)cp;
    cp = trace_get_payload_from_ip6(ip6, &proto, &left);
    addr_len = INET6_ADDRSTRLEN;
  } else {
    log_dbg("not IPv4 or IPv6");
    return msg;
  }

  if (cp == NULL || left == 0) {
    log_dbg("no payload left");
    return msg;
  }

  uint8_t left_limit = 0;
  if (proto == TRACE_IPPROTO_UDP) {
    if (left < sizeof(libtrace_udp_t)) {
      log_dbg("no bytes left for udp header");
      return msg;
    }
    udp = (libtrace_udp_t *)cp;
    cp = trace_get_payload_from_udp(udp, &left);
  } else if (proto == TRACE_IPPROTO_TCP) {
    if (left < sizeof(libtrace_tcp_t)) {
      log_dbg("no bytes left for tcp header");
      return msg;
    }
    tcp = (libtrace_tcp_t *)cp;
    cp = trace_get_payload_from_tcp(tcp, &left);
    left_limit = 2;
  } else  {
    log_dbg("not TCP or UDP");
    return msg;
  }
  uint8_t dns_header_len = 12;
  if (cp == NULL || left <= left_limit + dns_header_len) {
    log_dbg("no payload left");
    return msg;
  }

  uint8_t *dns_payload = (uint8_t *)cp;
  left -= left_limit;
  dns_payload += left_limit;

  if (!is_query(dns_payload, left, false)) {//only process queries
    log_dbg("pakcet from trace is not query");
    return msg;
  }
  
  //almost ok
  struct timeval ts = trace_get_timeval(packet);
  msg->set_seconds(ts.tv_sec);
  msg->set_microseconds(ts.tv_usec);
  msg->set_tcp(proto == TRACE_IPPROTO_TCP);
  msg->set_ipv4(ethertype == TRACE_ETHERTYPE_IP);

  char *ip_str = new char[addr_len];
  
  memset(ip_str, 0, addr_len);
  if (trace_get_source_address_string(packet, ip_str, addr_len) == NULL) {
    warnx("no valid source ip address");
    return msg;
  }
  msg->set_src_ip((const char *)ip_str, strlen(ip_str));

  memset(ip_str, 0, addr_len);
  if (trace_get_destination_address_string(packet, ip_str, addr_len) == NULL) {
    warnx("no valid destination ip address");
    return msg;
  };
  msg->set_dst_ip((const char *)ip_str, strlen(ip_str));

  delete[] ip_str;

  LOG(LOG_DBG, "[%d] %ld.%06d %s: %s:%d->%s:%d\n", my_pid, msg->seconds(), msg->microseconds(),
      msg->tcp()?"TCP":"UDP", msg->src_ip().c_str(), src_port, msg->dst_ip().c_str(), dst_port);

  msg->set_raw((const char *)dns_payload, (int)left);

  return msg;
}

void InputSource::log_dbg(const char *s)
{
  LOG(LOG_DBG, "[%d] %s\n", my_pid, s);
}
