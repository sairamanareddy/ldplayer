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

#include "check.hh"
#include "utility.hh"
#include <err.h>
using namespace std;

void check_lt0(string s1, const char *s)
{
  if (!lt0(s1))
    errx(1, "[error] %s must be < 0, %s is invalid, abort!", s, s1.c_str());
}

void check_lt0(char *s1, const char *s)
{
  string tmp = s1;
  check_lt0(tmp, s);
}

void check_gt0(string s1, const char *s)
{
  if (!gt0(s1))
    errx(1, "[error] %s must be > 0, %s is invalid, abort!", s, s1.c_str());
}

void check_gt0(char *s1, const char *s)
{
  string tmp = s1;
  check_gt0(tmp, s);
}

void check_int(int i, const char *s)
{
  if (i <= 0)
    errx(1, "[error] %s[%d] is invalid, abort!", s, i);
}

void check_int(string &s1, const char *s)
{
  if (s1.length() == 0 || stoi(s1) < 0)
    errx(1, "[error] %s[%s] is invalid, abort!", s, s1.c_str());
}

void check_str(string &s1, const char *s)
{
  if (s1.length() == 0)
    errx(1, "[error] %s is invalid, abort!", s);
}

void check_str(string &s1, string &s2, const char *s)
{
  if (s1.length() == 0 || s2.length() == 0)
    errx(1, "[error] %s is invalid, abort!", s);
}

void check_set(string &s1, const set<string> &ss, const char *s)
{
  if (ss.find(s1) == ss.end())
    errx(1, "[error] %s is invalid, abort!", s);
}

void check_map(string &s1, const unordered_map<string, unsigned int> &m, const char *s)
{
  if (m.find(s1) == m.end()) {
    string e;
    bool first = true;
    for (auto it : m) {
      if (first) {
	first = false;
      } else {
	e += ',';
      }
      e += "\'" + it.first + "\'";
    }
    errx(1, "[error] %s is invalid; available options:%s", s, e.c_str());
  }
}
