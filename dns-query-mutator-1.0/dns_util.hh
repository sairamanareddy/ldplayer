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
 */

#ifndef DNS_UTIL_HH
#define DNS_UTIL_HH

#include <stdint.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <ldns/ldns.h>

#define DNS_NONE            0x00000000u
#define DNS_OPCODE          0x00000001u
#define DNS_AA              0x00000002u
#define DNS_TC              0x00000004u
#define DNS_RD              0x00000008u
#define DNS_RA              0x00000010u
#define DNS_Z               0x00000020u
#define DNS_AD              0x00000040u
#define DNS_CD              0x00000080u
#define EDNS_DO             0x00000100u
#define EDNS_UDP_SIZE       0x00000200u
#define EDNS_EXTENDED_RCODE 0x00000400u
#define EDNS_VERSION        0x00000800u
#define EDNS_Z              0x00001000u

extern std::unordered_map<std::string, unsigned int> DNS_OPT_MAP;
extern std::vector<std::string> DNS_MSG_HEADER;

uint16_t get_id(uint8_t *);

void set_random_id(uint8_t *);
void print_dns_pkt(uint8_t *, int);

bool is_query(uint8_t *, size_t, bool);
bool dns_msg_str (uint8_t *, size_t, std::string &);
bool build_dns_pkt(uint8_t **, size_t *);
bool build_dns_pkt_query(uint8_t **, size_t *, bool, std::string, std::string, std::string);
bool change_dns_pkt(uint8_t **, size_t *, uint8_t *, size_t, std::unordered_map<uint32_t, uint32_t> &);

bool get_query_rr_str(ldns_pkt *, std::string &);
std::string get_query_rr_str(uint8_t *, size_t);

#endif //DNS_UTIL_HH
