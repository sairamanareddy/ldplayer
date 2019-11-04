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
 */

#ifndef CHECK_HH
#define CHECK_HH
#include <string>
#include <set>
#include <unordered_map>

void check_lt0(std::string, const char *);
void check_lt0(char *, const char *);
void check_gt0(std::string, const char *);
void check_gt0(char *, const char *);
void check_int(int, const char *);
void check_int(std::string &, const char *);
void check_str(std::string &, const char *);
void check_str(std::string &, std::string &, const char *);
void check_set(std::string &, const std::set<std::string> &, const char *);
void check_map(std::string &, const std::unordered_map<std::string, unsigned int> &, const char *);

#endif //CHECK_HH
