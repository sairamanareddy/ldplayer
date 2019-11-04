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

#ifndef LZ_STR_UTIL_HH
#define LZ_STR_UTIL_HH

#include <string>
#include <vector>
#include <unordered_set>

void trim_space(std::string &);
std::string trim_space2(std::string);

void str_split(std::string &, std::vector<std::string> &);
void str_split(std::string, std::string &, std::string &, char);
void str_split(std::string &, std::vector<std::string> &, char, bool);

bool str_set(std::string &, const std::unordered_set<std::string> &, const char *msg);

bool is_number(std::string, bool);

#endif //LZ_STR_UTIL_HH
