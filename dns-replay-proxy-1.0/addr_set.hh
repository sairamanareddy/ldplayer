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

#ifndef ADDR_SET_HH
#define ADDR_SET_HH

#include <netinet/in.h>
#include <string>
#include <cstdint>

class addr_set{
public:
  addr_set(std::string, std::string, uint16_t);
  ~addr_set();

  struct sockaddr *addr(bool);
  size_t addr_size(bool);
  struct sockaddr_in *v4addr();
  struct sockaddr_in6 *v6addr();

  std::string str(bool);
  const char *cstr(bool);

  uint16_t get_port();

private:
  uint16_t port;
  std::string v4_str;
  std::string v6_str;
  struct sockaddr_in *v4_addr;
  struct sockaddr_in6 *v6_addr;
};
 
#endif //ADDR_SET_HH
