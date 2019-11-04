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
 *
 */

#include <stdio.h>
#include <err.h>
#include <ldns/ldns.h>
#include "dns_util.hh"
#include <netinet/in.h>
#include <iostream>
#include <vector>
using namespace std;

bool build_dns_pkt(uint8_t **pkt, size_t *pkt_size)
{
  ldns_pkt *p = NULL;
  p = ldns_pkt_new();
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

bool build_dns_query(uint8_t **pkt, size_t *pkt_size, query_t *q)
{
  ldns_pkt *p = NULL;
  ldns_rr_type rr_type = ldns_get_rr_type_by_name(q->qtype);
  ldns_rr_class rr_class = ldns_get_rr_class_by_name(q->qclass);
  ldns_status s;
  s = ldns_pkt_query_new_frm_str(&p, q->qname, rr_type, rr_class,
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

  //set up EDNS
  ldns_pkt_set_edns_do(p, 1);
  ldns_pkt_set_edns_udp_size(p, 4096);
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

void set_random_id (uint8_t *buf)
{
  uint16_t id = ldns_get_random();
  ldns_write_uint16(buf, id);
}

uint16_t get_id (uint8_t *buf)
{
  return ldns_read_uint16(buf);
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
