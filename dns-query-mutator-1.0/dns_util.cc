/*
 * Copyright (C) 2017 by the University of Southern California
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
 *
 */

#include <stdio.h>
#include <err.h>
#include "dns_util.hh"
#include <netinet/in.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <unordered_set>

using namespace std;

unordered_map<string, unsigned int> DNS_OPT_MAP = {
  {"dns-opcode",            DNS_OPCODE},
  {"dns-aa",                DNS_AA},
  {"dns-tc",                DNS_TC},
  {"dns-rd",                DNS_RD},
  {"dns-ra",                DNS_RA},
  {"dns-z",                 DNS_Z},
  {"dns-ad",                DNS_AD},
  {"dns-cd",                DNS_CD},
  {"edns-do",               EDNS_DO},
  {"edns-udp-size",         EDNS_UDP_SIZE},
  {"edns-extended-rcode",   EDNS_EXTENDED_RCODE},
  {"edns-version",          EDNS_VERSION},
  {"edns-z",                EDNS_Z}
};

vector<string> DNS_MSG_HEADER = {
  //"time",
  //"srcip",
  //"srcport",
  //"dstip",
  //"dstport",
  //"protocol",
  "id",
  "qr", "opcode", "aa", "tc", "rd", "ra", "z", "ad", "cd", "rcode",
  "qdcount", "ancount", "nscount", "arcount",
  "edns_do", "edns_udp_size", "edns_extended_rcode", "edns_version", "edns_z",
  "qname", "qclass", "qtype"
};

//is the packet a dns query?
bool is_query(uint8_t *buf, size_t buf_sz, bool is_tcp)
{
  uint16_t qr = 0x8000U;
  uint16_t bits = 0x0000U;
  uint8_t offset = 2;
  offset += (is_tcp ? 2 : 0);
  memcpy(&bits, buf + offset, sizeof(bits));
  bits = ntohs(bits);
  return ((qr & bits) == 0);
}

//print the dns packet
void print_dns_pkt(uint8_t *buf, int buf_sz)
{
  ldns_status s;
  ldns_pkt* pkt = NULL;
  s = ldns_wire2pkt(&pkt, buf, (size_t)buf_sz);
  if(s != LDNS_STATUS_OK) {
    ldns_pkt_free(pkt);
    errx(1, "cannot parse packet:%s", ldns_get_errorstr_by_id(s));
  }
  ldns_pkt_print(stdout, pkt);
  ldns_pkt_free(pkt);
}

//build a dns query packet without query content
//TODO: no edns by default? ldns_pkt_new UNSET all edns bits
bool build_dns_pkt(uint8_t **pkt, size_t *pkt_size)
{
  ldns_pkt *p = NULL;
  p = ldns_pkt_new(); //ldns/packet.c:ldns_pkt_new()=>ldns_pkt_set_qr(packet, false);
  if (!p) {
    cerr << "[error] ldns cannot create pakcet" << endl;
    ldns_pkt_free(p);
    return false;
  }
  ldns_pkt_set_random_id(p);
  ldns_status s = ldns_pkt2wire(pkt, p, pkt_size);
  if (s != LDNS_STATUS_OK) {
    cerr << "[error] " << ldns_get_errorstr_by_id(s) << endl;
    ldns_pkt_free(p);
    return false;
  }
  ldns_pkt_free(p);
  return true;
}

//build a dns query packet with query content
bool build_dns_pkt_query(uint8_t **pkt, size_t *pkt_size, bool edns,
			 string qname, string qclass, string qtype)
{
  ldns_pkt *p = NULL;
  ldns_rr_type rr_type = ldns_get_rr_type_by_name(qtype.c_str());
  ldns_rr_class rr_class = ldns_get_rr_class_by_name(qclass.c_str());
  ldns_status s;
  s = ldns_pkt_query_new_frm_str(&p, qname.c_str(), rr_type, rr_class,
				 (uint16_t)(LDNS_RD|LDNS_CD));//to chekc the bits
  if (s != LDNS_STATUS_OK) {
    cerr << "[error] " << ldns_get_errorstr_by_id(s) << endl;
    ldns_pkt_free(p);
    return false;
  }

  if (!p) {
    cerr << "[error] memory problem" << endl;
    ldns_pkt_free(p);
    return false;
  }

  //ldns/packet.c: ldns_pkt_query_new_frm_str calls ldns_pkt_new which does not set edns
  if (edns) { //set up EDNS
    ldns_pkt_set_edns_do(p, 1);
    ldns_pkt_set_edns_udp_size(p, 4096);
  }
  
  ldns_pkt_set_random_id(p);
  s = ldns_pkt2wire(pkt, p, pkt_size);
  if (s != LDNS_STATUS_OK) {
    cerr << "[error] " << ldns_get_errorstr_by_id(s) << endl;
    ldns_pkt_free(p);
    return false;
  }
  ldns_pkt_free(p);
  return true;
}

// use Macros in wire2host.h on wire data directly
// no need to convert to ldns packet
bool change_dns_pkt_header (uint8_t *buf, size_t buf_sz, uint32_t part, uint32_t val)
{
  switch(part){
  case DNS_OPCODE:
    LDNS_OPCODE_SET(buf, val);
    break;
  case DNS_AA:
    if (val == 0) LDNS_AA_CLR(buf);
    else LDNS_AA_SET(buf);
    break;
  case DNS_TC:
    if (val == 0) LDNS_TC_CLR(buf);
    else LDNS_TC_SET(buf);
    break;
  case DNS_RD:
    if (val == 0) LDNS_RD_CLR(buf);
    else LDNS_RD_SET(buf);
    break;
  case DNS_RA:
    if (val == 0) LDNS_RA_CLR(buf);
    else LDNS_RA_SET(buf);
    break;
  case DNS_Z:
    if (val == 0) LDNS_Z_CLR(buf);
    else LDNS_Z_SET(buf);
    break;
  case DNS_AD:
    if (val == 0) LDNS_AD_CLR(buf);
    else LDNS_AD_SET(buf);
    break;
  case DNS_CD:
    if (val == 0) LDNS_CD_CLR(buf);
    else LDNS_CD_SET(buf);
    break;
  default:
    warnx("unsupported dns header change option: %u", part);
    return false;
  }
  return true;
}

bool change_dns_pkt_edns (ldns_pkt* pkt, unsigned int part, uint32_t val)
{
  switch(part){
  case EDNS_DO:
    if (val > 0)
      ldns_pkt_set_edns_do(pkt, true);
    else { //unset all edns
      ldns_pkt_set_edns_do(pkt, false);
      ldns_pkt_set_edns_udp_size(pkt, 0);
      ldns_pkt_set_edns_extended_rcode(pkt, 0);
      ldns_pkt_set_edns_version(pkt, 0);
      ldns_pkt_set_edns_z(pkt, 0);
      ldns_pkt_set_edns_data(pkt, NULL);
      //pkt->_edns_present = false;//need for ldns>=1.7.0
    }
    break;
  case EDNS_UDP_SIZE:
    ldns_pkt_set_edns_udp_size(pkt, (uint16_t)val);
    break;
  case EDNS_EXTENDED_RCODE:
    ldns_pkt_set_edns_extended_rcode(pkt, (uint8_t)val);
    break;
  case EDNS_VERSION:
    ldns_pkt_set_edns_version(pkt, (uint8_t)val);
    break;
  case EDNS_Z:
    ldns_pkt_set_edns_z(pkt, (uint16_t)val);
    break;
  default:
    warnx("unsupported change option: %u", part);
    return false;
  }
  return true;
}

bool change_dns_pkt(uint8_t **raw, size_t *raw_sz, uint8_t *raw_old, size_t raw_old_sz,
                    unordered_map<uint32_t, uint32_t> &opt)
{
  assert(raw_old);
  assert(raw_old_sz > 0);
  
  // make a buf to change packet, better NOT change raw_old
  uint8_t *buf = new uint8_t[raw_old_sz];
  size_t buf_sz = raw_old_sz;
  memset(buf , 0, buf_sz);
  memcpy(buf, raw_old, buf_sz);

  // for edns change
  ldns_status s;
  ldns_pkt* pkt = NULL;
  
  // for header bits, use ldns Macros, do NOT convert to ldns_pkt
  // Can ldns parse weird dns query names?
  unordered_map<uint32_t, uint32_t> edns_opt;
  bool edns_ok = true, header_ok = true;
  for (auto o : opt) {
    uint32_t part = o.first;
    uint32_t val = o.second;
    if (part >= EDNS_DO && part <= EDNS_Z && (part & (part-1)) == 0) {//change edns
      edns_opt[part] = val; //do not change here, we will do it later
    } else if (part >= DNS_OPCODE && part <= DNS_CD && (part & (part-1)) == 0) {//header bits
      header_ok = (header_ok && change_dns_pkt_header(buf, buf_sz, part, val));
    } else {
      warnx("unknown dns change option: %u", part);
    }
  }

  // no change for edns, clean up and return
  if (edns_opt.empty()) {
    *raw = new uint8_t[buf_sz];
    *raw_sz = buf_sz;
    memset(*raw , 0, buf_sz);
    memcpy(*raw, buf, buf_sz);
    delete[] buf;
    return header_ok;
  }
  
  // ok we need to change edns, make a ldns packet first
  s = ldns_wire2pkt(&pkt, buf, (size_t)buf_sz);
  delete[] buf;
  if(s != LDNS_STATUS_OK) {
    ldns_pkt_free(pkt);
    errx(1, "cannot parse packet:%s", ldns_get_errorstr_by_id(s));
  }
  for (auto o : edns_opt)
    edns_ok = (edns_ok && change_dns_pkt_edns(pkt, o.first, o.second));

  // done edns, convert ldns packet to raw
  s = ldns_pkt2wire(raw, pkt, raw_sz);
  if (s != LDNS_STATUS_OK) {
    cerr << "[error] " << ldns_get_errorstr_by_id(s) << endl;
    ldns_pkt_free(pkt);
    return false;
  }
  
  ldns_pkt_free(pkt);
  return header_ok && edns_ok;
  
  /*  switch(part){
  case DNS_OPCODE:
    ldns_pkt_set_opcode(pkt, (ldns_pkt_opcode)val);
    break;
  case DNS_AA:
    ldns_pkt_set_aa(pkt, (bool)val);
    break;
  case DNS_TC:
    ldns_pkt_set_tc(pkt, (bool)val);
    break;
  case DNS_RD:
    ldns_pkt_set_rd(pkt, (bool)val);
    break;
  case DNS_RA:
    ldns_pkt_set_ra(pkt, (bool)val);
    break;
  case DNS_Z:
    break;
  case DNS_AD:
    ldns_pkt_set_ad(pkt, (bool)val);
    break;
  case DNS_CD:
    ldns_pkt_set_cd(pkt, (bool)val);
    break;
  case EDNS_DO:
    ldns_pkt_set_edns_do(pkt, (bool)val);
    break;
  case EDNS_UDP_SIZE:
    ldns_pkt_set_edns_udp_size(pkt, (uint16_t)val);
    break;
  case EDNS_EXTENDED_RCODE:
    ldns_pkt_set_edns_extended_rcode(pkt, (uint8_t)val);
    break;
  case EDNS_VERSION:
    ldns_pkt_set_edns_version(pkt, (uint8_t)val);
    break;
  case EDNS_Z:
    ldns_pkt_set_edns_z(pkt, (uint16_t)val);
    break;    
  default:
    warnx("unsupported change option");
  }*/
}

bool dns_msg_str (uint8_t *buf, size_t buf_sz, std::string &line)
{
  ldns_pkt* m = NULL;
  ldns_status s = ldns_wire2pkt(&m, buf, buf_sz);
  if(s != LDNS_STATUS_OK) {
    ldns_pkt_free(m);
    warnx("dns_msg_str: cannot parse packet:%s", ldns_get_errorstr_by_id(s));
    return false;
  }
  string query_rr, query_rr_fmt; 
  if (!get_query_rr_str(m, query_rr)) {//do NOT return false here, packet may be malformatted
    warnx("cannot get query rr string");
    ldns_pkt_print(stderr, m);
  }
  istringstream iss(query_rr);
  while (!iss.eof()) {
    string x;
    iss >> x;
    if (!x.empty()) {
      if (!query_rr_fmt.empty())
	query_rr_fmt += "\t";
      query_rr_fmt += x;
    }
  }
  if (query_rr_fmt.empty()) query_rr_fmt = "-\t-\t-";
  
  stringstream ss;
  int edns_version = ldns_pkt_edns_version(m);
  int ends_extended_rcode = ldns_pkt_edns_extended_rcode(m);

  ss << ldns_pkt_id(m) << "\t"
     << ldns_pkt_qr(m) << "\t" << ldns_pkt_get_opcode(m) << "\t"
     << ldns_pkt_aa(m) << "\t" << ldns_pkt_tc(m) << "\t"
     << ldns_pkt_rd(m) << "\t" << ldns_pkt_ra(m) << "\t"
     << LDNS_Z_WIRE(buf) << "\t"
     << ldns_pkt_ad(m) << "\t" << ldns_pkt_cd(m) << "\t"
     << ldns_pkt_get_rcode(m) << "\t"
     << ldns_pkt_qdcount(m) << "\t" << ldns_pkt_ancount(m) << "\t"
     << ldns_pkt_nscount(m) << "\t" << ldns_pkt_arcount(m) << "\t"
     << ldns_pkt_edns_do(m) << "\t" << ldns_pkt_edns_udp_size(m) << "\t"
     << ends_extended_rcode << "\t" << edns_version << "\t" << ldns_pkt_edns_z(m) << "\t"
     << query_rr_fmt;

  line = ss.str();
  ldns_pkt_free(m);
  return true;
}

void set_random_id (uint8_t *buf)
{
  uint16_t id = ldns_get_random();
  ldns_write_uint16(buf, id);
}

uint16_t get_id (uint8_t *buf)
{
  return ldns_read_uint16(buf);
}

bool get_query_rr_str(ldns_pkt *pkt, string &s)
{
  if (!pkt || ldns_pkt_qdcount(pkt) <= 0) return false;
  ldns_rr *q = ldns_rr_list_rr(ldns_pkt_question(pkt), 0);
  assert(q);
  char *str = ldns_rr2str(q);
  assert(str);
  s = str;
  free(str);
  return true;
}

string get_query_rr_str(uint8_t *buf, size_t buf_sz)
{
  string r;
  ldns_status s;
  ldns_pkt* pkt = NULL;
  s = ldns_wire2pkt(&pkt, buf, buf_sz);
  if(s != LDNS_STATUS_OK) {
    ldns_pkt_free(pkt);
    warnx("cannot parse packet:%s", ldns_get_errorstr_by_id(s));
    return r;
  }

  if (ldns_pkt_qdcount(pkt) <= 0) {
    ldns_pkt_free(pkt);
    return r;
  }

  ldns_rr *q = ldns_rr_list_rr(ldns_pkt_question(pkt), 0);
  char *str = ldns_rr2str(q);
  r = str;
  free(str);
  ldns_pkt_free(pkt);
  return r;
}

