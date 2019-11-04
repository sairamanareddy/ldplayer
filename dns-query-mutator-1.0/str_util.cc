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

#include "str_util.hh"
#include <iostream>
#include <sstream>
#include <cctype>	//for isdigit
#include <cstring>	//for strlen
#include <err.h>        //for warnx
using namespace std;

// NOTE: all the functions should not exit, let the caller make
// decisions based on return values

//remove leading and tailing spaces
void trim_space(string &s) {
  stringstream trimmer;
  trimmer << s;
  s.clear();
  trimmer >> s;
}

string trim_space2(string s) {
  trim_space(s);
  return s;
}

//does s1 in ss?
bool str_set(string &s1, const unordered_set<string> &ss, const char *msg)
{
  string tmp;
  for (string s : ss) {
    if (!tmp.empty()) tmp += ", ";
    tmp += s;
  }
  if (ss.find(s1) == ss.end()) {
    if (strlen(msg) > 0)
      cerr << "[error] " << msg <<" '"<< s1 << "' is not in [" << tmp << "]" << endl;
    return false;
  } else {
    return true;
  }
}

//split string to two sub-string
void str_split(string s, string &s1, string &s2, char c)
{
  size_t found = s.find_first_of(c);
  if (found == string::npos)
    errx(1, "[error] '%s' does not contain '%c'!", s.c_str(), c);
  s1 = s.substr(0, found);
  s2 = s.substr(found + 1);
}

//split a string to vector
void str_split(string &s, vector<string> &substrs)
{
  if (s.empty()) return;
  istringstream ss(s);
  while(!ss.eof()) {
    string x;
    ss >> x;
    trim_space(x);
    if (!x.empty())
      substrs.push_back(x);
  }
}

//split a string
//http://www.cplusplus.com/faq/sequences/strings/split/#getline
void str_split(string &s, vector<string> &substrs, char c, bool trim)
{
  if (s.empty()) return;
  istringstream ss(s);
  while (!ss.eof()) {
    string x;
    getline(ss, x, c);
    if (trim) trim_space(x);
    substrs.push_back(x);
  }
}

//test a string is number or not
bool is_number(string s, bool positive)
{
  trim_space(s);
  if (s.empty()) return false;
  auto it = s.begin();
  if (positive) {
    if (*it == '-') return false;
    if (*it == '+') it++;
  } else {
    if (*it != '-') return false;
    if (*it == '-') it++;
  }
  while (it < s.end()) {
    if(!isdigit(*it)) return false;
    it++;
  }
  return true;
}
