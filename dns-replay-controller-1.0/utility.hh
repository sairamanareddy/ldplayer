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

#ifndef LZ_UTIL_HH
#define LZ_UTIL_HH

#include <vector>
#include <string>
#include <sys/time.h>	/* gettimeofday */

#define UTIL_MAX(a, b)  (((a) > (b)) ? (a) : (b))

void int2str(int, std::string &);
bool is_number(std::string & );
bool lt0(std::string);
bool gt0(std::string);
bool is_number(std::string, bool);
void check_path(const char *, bool);
void str_split(std::string &, std::vector<std::string> &);
void str_split(std::string &, std::vector<std::string> &, char, bool);
void str_split(std::string, std::string &, std::string &, char);
void trim_spaces(std::string &);

double get_time_now(struct timeval *, struct timezone *, std::string);
double get_time_now(std::string);
void get_time_now(struct timeval *);

std::string rm_last_dot(std::string);
std::string str_tolower(std::string);

std::string fmt_str(std::string, std::string);

void die(const char *msg);
#endif	//UTILITY_HH
