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

#ifndef PROXY_HH
#define PROXY_HH

#include <netinet/in.h>
#include <string>
#include <vector>
#include <thread>
#include "safe_queue.hh"
#include "addr_set.hh"

class DNSReplayProxy{
public:
  DNSReplayProxy(std::string, std::string, std::string, std::string, int);
  ~DNSReplayProxy();
  void start();

private:
  addr_set *aut_addr;
  addr_set *rec_addr;
  addr_set *target_addr;

  int tun_fd;
  int num_threads;

  std::vector<std::thread> threads;
  std::vector<int> raw_fd;
  std::string tun_name;
  std::string proxy_type;
  safe_queue<std::tuple<uint8_t *, int> > *packet_queue;
  
  void process_packet(int);
  void read_packet();
};

#endif //PROXY_HH
