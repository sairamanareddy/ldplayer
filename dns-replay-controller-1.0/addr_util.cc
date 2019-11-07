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

#include "addr_util.h"
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string>
#include <err.h> //for err

//is it IPv4 address?
bool ipv4_addr(const char *s)
{
  struct sockaddr_in sa;
  return inet_pton(AF_INET, s, &(sa.sin_addr)) != 0;
}

//is it IPv6 address?
bool ipv6_addr(const char *s)
{
  struct sockaddr_in6 sa;
  return inet_pton(AF_INET6, s, &(sa.sin6_addr)) != 0;
}

//IP binary to string
char *ip_b2s(const struct sockaddr *sa, char *s, size_t maxlen)
{
  const char *rv = NULL;
  switch(sa->sa_family) {
  case AF_INET:
    rv = inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr), s, maxlen);
    if (rv == NULL)
      err(1, "[error] inet_ntop(AF_INET) fails, abort!");
    break;
  case AF_INET6:
    rv = inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr), s, maxlen);
    if (rv == NULL)
     err(1, "[error] inet_ntop(AF_INET6) fails, abort!");
    break;
  default:
    errx(1, "[error] AF is unknown!");
    return NULL;
  }
  return s;
}

//IP string to binary
int ip_s2b(const char *ip_v, const char *s, const struct sockaddr *sa)
{
  int rv = -1;
  if (ip_v == NULL) {
    errx(1, "input for ip_s2b is NULL");
    return -1; 
  }
  if (strcmp(ip_v, "ipv4") == 0)
    rv = inet_pton(AF_INET, s, &(((struct sockaddr_in *)sa)->sin_addr));
  else if (strcmp(ip_v, "ipv6") == 0)
    rv = inet_pton(AF_INET6, s, &(((struct sockaddr_in6 *)sa)->sin6_addr));
  else
    errx(1, "ip version [%s] is unknow in ip_s2b", ip_v);
  if (rv != 1)
    errx(1, "ip_s2b [%s] fails", s);
  return rv;
}

//get ip address string from sockaddr
int get_in_addr(struct sockaddr *sa, char **s)
{
  void *a = NULL;
  int len = INET_ADDRSTRLEN;
  if (sa->sa_family == AF_INET)
    a = &(((struct sockaddr_in *)sa)->sin_addr);
  else {
    a = &(((struct sockaddr_in6 *)sa)->sin6_addr);
    len = INET6_ADDRSTRLEN;
  }
  
  *s = new char[len];
  const char *r = inet_ntop(sa->sa_family, a, *s, len);
  if (r == NULL) {
    warn("cannot get IP address string from socket address");
    delete *s;
    len = -1;
  }
  return len;
}

//get port from sockaddr
uint16_t get_in_port(struct sockaddr *sa)
{
  uint16_t port = 0;
  if (sa->sa_family == AF_INET)
    port = ((struct sockaddr_in *)sa)->sin_port;
  else
    port = ((struct sockaddr_in6 *)sa)->sin6_port;
  return ntohs(port);
}

void fill_addr(struct addrinfo **addr, const char *ip, int port)
{
  *addr = NULL;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = AI_NUMERICSERV;
  hints.ai_family = AF_UNSPEC;
  struct sockaddr_in6 serveraddr;
  if(inet_pton(AF_INET, ip, &serveraddr)){
    //ipv4 address
    hints.ai_family = AF_INET;
    hints.ai_flags |= AI_NUMERICHOST;
  }

  else if(inet_pton(AF_INET6, ip, &serveraddr)){
    //ipv6 address
    hints.ai_family = AF_INET6;
    hints.ai_flags |= AI_NUMERICHOST;
  }

  else{
    err(1, "[error] ip [%s] is invalid, abort!", ip);
  }

  int rc = getaddrinfo(ip, std::to_string(port).c_str(), &hints, addr);
  if(rc!=0){
    if(rc==EAI_SYSTEM){
    err(1, "[error] getaddrinfo() failed");
    }
  }
  assert((*addr) != NULL);
}
