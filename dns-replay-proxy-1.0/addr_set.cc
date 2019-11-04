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

#include "addr_set.hh"
#include "addr_util.h"
#include <err.h> //for err
#include <cstring>
using namespace std;

addr_set::addr_set(string v4s, string v6s, uint16_t p)
{
  if (!ipv4_addr(v4s.c_str()))
    errx(1, "[error] [%s] is not a valid IPv4 address!", v4s.c_str());
  if (!ipv6_addr(v6s.c_str()))
    errx(1, "[error] [%s] is not a valid IPv6 address!", v6s.c_str());
  if (p == 0)
    errx(1, "[error] port number must be > 0!");

  v4_str = v4s;
  v6_str = v6s;
  port = p;
  v4_addr = NULL;
  v6_addr = NULL;

  v4_addr = new struct sockaddr_in;
  if (!v4_addr) 
    errx(1, "[error] memory allocation failure!");
  memset(v4_addr, 0, sizeof(struct sockaddr_in));
  v4_addr->sin_family = AF_INET;
  v4_addr->sin_port = htons(p);
  ip_s2b("ipv4", v4s.c_str(), (struct sockaddr *)v4_addr);

  v6_addr = new struct sockaddr_in6;
  if (!v6_addr) 
    errx(1, "[error] memory allocation failure!");
  memset(v6_addr, 0, sizeof(struct sockaddr_in6));
  v6_addr->sin6_family = AF_INET6;
  v6_addr->sin6_port = htons(p);
  ip_s2b("ipv6", v6s.c_str(), (struct sockaddr *)v6_addr);
}

addr_set::~addr_set()
{
  if (v4_addr != NULL)
    delete v4_addr;
  if (v6_addr != NULL)
    delete v6_addr;
}

uint16_t addr_set::get_port()
{
  return port;
}

string addr_set::str(bool v4)
{
  return v4 ? v4_str:v6_str;
}

const char *addr_set::cstr(bool v4)
{
  return v4 ? v4_str.c_str() : v6_str.c_str();
}

struct sockaddr_in *addr_set::v4addr()
{
  return v4_addr;
}

struct sockaddr_in6 *addr_set::v6addr()
{
  return v6_addr;
}

struct sockaddr *addr_set::addr(bool v4)
{
  if (v4) 
    return (struct sockaddr *)v4_addr;
  else
    return (struct sockaddr *)v6_addr;
}

size_t addr_set::addr_size(bool v4)
{
  if (v4) 
    return sizeof(*v4_addr);
  else
    return sizeof(*v6_addr);
}
