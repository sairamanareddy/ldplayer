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

#ifndef ADDR_UTIL_H
#define ADDR_UTIL_H

#include <netinet/in.h>

bool ipv4_addr(const char *);
bool ipv6_addr(const char *);
int ip_s2b(const char *, const char *, const struct sockaddr *);
char *ip_b2s(const struct sockaddr *, char *, size_t);

int get_in_addr(struct sockaddr *, char **);
uint16_t get_in_port(struct sockaddr *);

#endif //ADDR_UTIL_H
