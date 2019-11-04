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

#ifndef DNS_UTIL_HH
#define DNS_UTIL_HH

#include <stdint.h>
#include <string>

struct query_t
{
  const char *qname;
  const char *qtype;
  const char *qclass;
};

bool build_dns_query(uint8_t **, size_t *, query_t *);
bool build_dns_pkt(uint8_t **, size_t *);
void print_dns_pkt(uint8_t *, int);
bool is_query(uint8_t *, size_t, bool);
void set_random_id(uint8_t *);
uint16_t get_id(uint8_t *);

std::string get_query_rr_str(uint8_t *, size_t);

#endif //DNS_UTIL_HH
