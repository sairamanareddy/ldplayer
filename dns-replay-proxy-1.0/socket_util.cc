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

#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
//#include <netinet/ip.h>
//#include <netinet/udp.h>
//#include <netinet/tcp.h>
#include "addr_util.h"
#include <err.h>
#include <string.h>
#include "socket_util.h"
#include <arpa/inet.h>

//#define MAX_PKT_LEN 4096

//This is copied from https://tools.ietf.org/html/rfc1071
uint16_t 
cksum (uint16_t *addr, int count)
{
  uint16_t checksum = 0;
  /* Compute Internet Checksum for "count" bytes
   *         beginning at location "addr".
   */
  register long sum = 0;
  
  while( count > 1 )  {
    /*  This is the inner loop */
    sum += *addr++;
    count -= 2;
  }
  
  /*  Add left-over byte, if any */
  if( count > 0 )
    sum += *addr;
  
  /*  Fold 32-bit sum to 16 bits */
  while (sum>>16)
    sum = (sum & 0xffff) + (sum >> 16);

  checksum = ~sum;
  return checksum;
}

//create a raw socket, udp or tcp
int
create_raw_socket(int one, bool v4flag)
{
  int raw = -1;
  int __domain = v4flag ? AF_INET : AF_INET6;
  int __level = v4flag ? IPPROTO_IP : IPPROTO_IPV6;
  if ((raw = socket(__domain, SOCK_RAW, IPPROTO_RAW)) < 0)
    err(1, "SOCK_RAW");
  if (setsockopt(raw, __level, IP_HDRINCL, &one, sizeof(one)) < 0)
    err(1, "setsockopt IP_HDRINCL");
  if (!v4flag && (setsockopt(raw, __level, IPV6_V6ONLY, &one, sizeof(one)) < 0))
    err(1, "setsockopt IPV6_V6ONLY");
  return raw;
}

//udp_cksum
uint16_t
udp_tcp_cksum (uint8_t *pkt, uint16_t len, struct in_addr ip_src, struct in_addr ip_dst, uint8_t protocol)
{
  pseudo_hdr fake_hdr;
  memset(&fake_hdr, 0, sizeof(pseudo_hdr));
  fake_hdr.ip_src.s_addr = ip_src.s_addr;
  fake_hdr.ip_dst.s_addr = ip_dst.s_addr;
  fake_hdr.reserved = 0;
  fake_hdr.protocol = protocol;
  fake_hdr.length = htons(len);
  int padding = 0;
  if (len % 2 != 0)
    padding = 1;
  int buf_len = len + sizeof(pseudo_hdr) + padding;
  uint8_t *buf = new uint8_t[buf_len];
  memset(buf, 0, buf_len);
  memcpy(buf, &fake_hdr, sizeof(pseudo_hdr));
  memcpy(buf + sizeof(pseudo_hdr), pkt, len);
  uint16_t sum = cksum((uint16_t*)buf, buf_len);
  delete[] buf;
  return sum;
}

//make a connection either for udp or tcp
int mk_conn (const char *ip, int port, bool tcp)
{
  struct addrinfo* sa;
  fill_addr(&sa, ip, port);
  int fd = -1;
  if((fd = socket(sa->ai_family, tcp ? SOCK_STREAM : SOCK_DGRAM, 0)) == -1)
    err(1, "socket in mk_conn");
  if (connect (fd, sa->ai_addr, sa->ai_addrlen) < 0)
    err(1, "connect in mk_conn");
  return fd;
}

//clean up fd
void clean_fd(int f)
{
  if (f > 0)
    close(f);
}
