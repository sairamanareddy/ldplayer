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

#include "proxy.hh"
#include <iostream>
#include <arpa/inet.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>   //for err
#include <cstdlib> //for exit

#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#include <glog/logging.h>

#include "socket_util.h"
#include "addr_util.h"

using namespace std;

#define BUF_LEN 4096

DNSReplayProxy::DNSReplayProxy(string t, string a, string r, string p, int n): num_threads(n),tun_name(t),proxy_type(p)
{
  //check input
  if (tun_name.empty()) {
    errx(1, "[error] tunnel name [%s] is not valid", tun_name.c_str());
  }

  if (proxy_type.empty()) {
    errx(1, "[error] proxy type is empty.");
  }

  if (proxy_type != "recursive" && proxy_type != "authoritative") {
    errx(1, "[error] proxy type [%s] is not valid", proxy_type.c_str());
  }

  if (num_threads < 1) {
    errx(1, "[error] number of threads [%d] is not valid, abort!", num_threads);
  }

  tun_fd = -1;

  aut_addr = NULL;
  rec_addr = NULL;
  target_addr = NULL;
  packet_queue = NULL;
  
  //TODO IPv6 ::1, now it is IPv4 only
  aut_addr = new addr_set(a, "::1", 53);
  rec_addr = new addr_set(r, "::1", 53);

  if (proxy_type == "recursive") {
    target_addr = new addr_set(a, "::1", 53);
  } else if (proxy_type == "authoritative") {
    target_addr = new addr_set(r, "::1", 53);
  } else {
    errx(1, "[error] unknow proxy type [%s]", proxy_type.c_str());
  }

  packet_queue = new safe_queue<tuple<uint8_t *, int> > (-1); //unlimited size
  
  if (!aut_addr || !rec_addr || !target_addr || !packet_queue) {
    errx(1, "[error] memory allocation failure!");
  }

  VLOG(3) << "running as \'" << proxy_type << "\' proxy";
  VLOG(3) << "authoritative:\t" << aut_addr->str(true) << "\t"
	  << aut_addr->str(false) << "\tport " << aut_addr->get_port();
  VLOG(3) << "recursive:\t" << rec_addr->str(true) << "\t"
	  << rec_addr->str(false) << "\tport " << rec_addr->get_port();
  VLOG(3) << "target:\t" << target_addr->str(true) << "\t"
	  << target_addr->str(false) << "\tport " << target_addr->get_port();
}

DNSReplayProxy::~DNSReplayProxy()
{
  if (aut_addr) {
    delete aut_addr;
  }

  if (rec_addr) {
    delete rec_addr;
  }

  if (target_addr) {
    delete target_addr;
  }

  clean_fd(tun_fd);

  for (auto fd : raw_fd) {
    clean_fd(fd);
  }
}

/*
  the following function is copied from simpletun.c
  at http://backreference.org/2010/03/26/tuntap-interface-tutorial/
*/
int tun_alloc(char *dev, int flags) {

  struct ifreq ifr;
  int fd, err;
  char *clonedev = (char*)"/dev/net/tun";

  if( (fd = open(clonedev , O_RDWR)) < 0 ) {
    perror("Opening /dev/net/tun");
    return fd;
  }

  memset(&ifr, 0, sizeof(ifr));

  ifr.ifr_flags = flags;

  if (*dev) {
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
  }

  if( (err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0 ) {
    perror("ioctl(TUNSETIFF)");
    close(fd);
    return err;
  }

  strcpy(dev, ifr.ifr_name);

  return fd;
}

/*
  print basic pakcet information: addresses
*/
void print_pkt_info(uint8_t *pkt, int len)
{
  struct ip *ip = (struct ip *)pkt;
  char *ip_src = NULL;
  char *ip_dst = NULL;

  uint16_t ip_len = 0;
  bool ipv6 = false;

  if (ip->ip_v == 4) {
    ip_len = INET_ADDRSTRLEN;
  } else if (ip->ip_v == 6) {
    ipv6 = true;
    ip_len = INET6_ADDRSTRLEN;
  }
  ip_src = new char[ip_len];
  ip_dst = new char[ip_len];
  memset(ip_src, '\0', ip_len);
  memset(ip_dst, '\0', ip_len);

  inet_ntop(ipv6?AF_INET6:AF_INET, &(ip->ip_src), ip_src, ip_len);
  inet_ntop(ipv6?AF_INET6:AF_INET, &(ip->ip_dst), ip_dst, ip_len);
  VLOG(3) << "IPv" << ip->ip_v << "\tfrom: " << ip_src << "\tto: " << ip_dst << "\tlen=" << len;
}

/*
  process each incoming packet from tunnel interface
*/
void process_packet_helper (uint8_t *pkt_orig, int len, int raw_fd, 
			    addr_set *target_addr, unsigned long id)
{
  if (pkt_orig == NULL) {
    errx(1, "[%lu] [error] original pakcet pointer is NULL, abort!", id);
  }
  
  uint8_t pkt[len]; // = new uint8_t[len];
  if (pkt == NULL) {
    errx(1, "[%lu] [error] memory failure: pakcet pointer is NULL, abort!", id);
  }

  memset(pkt, 0, len);
  memcpy(pkt, pkt_orig, len);

  //before
  VLOG(3) << "[" << id << "] original:";
  print_pkt_info(pkt, len);

  struct ip *ip = (struct ip *)&pkt[0];
  if (ip->ip_v == 4) {
    ip->ip_src.s_addr = ip->ip_dst.s_addr;
    //ip->ip_dst.s_addr = target_addr->sin_addr.s_addr; //IPv4 only for now
    inet_pton(AF_INET, target_addr->cstr(true), &(ip->ip_dst));
    ip->ip_sum = 0; //let the kernel compute checksum
    //ip->ip_sum = cksum((uint16_t *)pkt, len);
  } else if (ip->ip_v == 6) {//TODO
    errx(1, "[error] IPv6 is not supported!");
    //inet_pton(AF_INET, target_addr->cstr(true), &(ip->ip_dst));
  }

  //after
  VLOG(3) << "[" << id << "] change address:";
  print_pkt_info(pkt, len);

  if(ip->ip_p == IPPROTO_ICMP) {
    VLOG(3) << "[" << id << "] ICMP: do nothing, only for testing";
  } else if(ip->ip_p == IPPROTO_TCP) {
    struct tcphdr *tcp = (struct tcphdr *)&pkt[20];
    VLOG(3) << "[" << id << "] TCP: src port=" << ntohs(tcp->source) << "\tdst port=" << ntohs(tcp->dest);
    tcp->check = htons(0);
    tcp->check = udp_tcp_cksum(&pkt[20], len-20, ip->ip_src, ip->ip_dst, ip->ip_p);
  } else if (ip->ip_p == IPPROTO_UDP) {
    struct udphdr *udp = (struct udphdr *)&pkt[20];
    VLOG(3) << "[" << id << "] UDP src port=" << ntohs(udp->source) << "\tdst port=" << ntohs(udp->dest)
	    << "\tudp len=" << ntohs(udp->len);
    udp->check = htons(0);
    udp->check = udp_tcp_cksum(&pkt[20], len-20, ip->ip_src, ip->ip_dst, ip->ip_p);
  }

  //IPv4 only for now
  ssize_t send_len = sendto(raw_fd, pkt, len, 0, target_addr->addr(true), target_addr->addr_size(true));
  if (send_len < 0) {
    err(1, "[%lu] [error] sendto raw return [%ld]", id, send_len);
  }
  VLOG(3) << "[" << id << "] send raw pkt, len=" << send_len;
}

/*
  read packets from tunnel and put them into queue
*/
void DNSReplayProxy::read_packet ()
{  
  uint8_t buf[BUF_LEN];
  tun_fd = tun_alloc((char *)tun_name.c_str(), IFF_TUN | IFF_NO_PI);
  if (tun_fd < 0) {
    err(1, "[error] cannot connect to tun[%s]\n", tun_name.c_str());
  }
  LOG(INFO) << "connect to tunnel interface=" << tun_name;
  int len = -1;
  unsigned long long pkt_count = 0;
  while (1) {
    memset(buf, 0, sizeof(buf));
    len = read(tun_fd, buf, sizeof(buf));
    if(len < 0) {
      close(tun_fd);
      err(1, "[error] reading tunnel, abort!\n");
    } else {
      pkt_count += 1;
      VLOG(3) << "packet " << pkt_count << " from tunnel, length=" << len;
      uint8_t *pkt = new uint8_t[len];
      memcpy(pkt, buf, len);
      packet_queue->push(tuple<uint8_t *,int> (pkt, len));
    }
  }
}

/*
  process the packets read from tunnel
*/
void DNSReplayProxy::process_packet (int raw_fd)
{
  tuple<uint8_t *, int> packet;
  hash<thread::id> hasher;
  unsigned long myid = hasher(this_thread::get_id());
  while (1) {
    packet = packet_queue->pop();
    uint8_t *data = get<0>(packet);
    int len = get<1>(packet);
    process_packet_helper(data, len, raw_fd, target_addr, myid);
    delete[] data;
  }
}

/*
  start to run the proxy
*/
void DNSReplayProxy::start()
{
  LOG(INFO) << "proxy starts";
  LOG(INFO) << "create 1 thread to read tunnel interface";
  threads.push_back(thread(&DNSReplayProxy::read_packet, this));
  
  LOG(INFO) << "create " << num_threads << " threads to process the packets";
  for (int i=0; i<num_threads; i++) {
    int fd = create_raw_socket(1);
    raw_fd.push_back(fd);
    threads.push_back(thread(&DNSReplayProxy::process_packet, this, fd));
  }
  LOG(INFO) << "join all threads";
  for (auto& th : threads) {
    th.join();
  }
  LOG(INFO) << "proxy ends";
}
