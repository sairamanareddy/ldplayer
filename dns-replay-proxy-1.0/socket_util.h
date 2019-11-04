/*
 * Copyright (C) 2015-2018 by the University of Southern California
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

#ifndef SOCKET_UTIL_H
#define SOCKET_UTIL_H

#include <stdint.h>
#include <netinet/in.h>

struct pseudo_hdr
{
  struct in_addr ip_src;
  struct in_addr ip_dst;
  uint8_t reserved;
  uint8_t protocol;
  uint16_t length;
};

int mk_conn (const char *, int, bool);
int create_raw_socket(int);

uint16_t cksum (uint16_t *, int);
uint16_t udp_tcp_cksum (uint8_t *, uint16_t, struct in_addr, struct in_addr, uint8_t);

void clean_fd(int);

#endif //SOCKET_UTIL_H
