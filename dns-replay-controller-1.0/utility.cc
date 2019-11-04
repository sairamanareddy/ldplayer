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

#include "utility.hh"
#include <iostream>
#include <cstdlib>	//for exit
#include <sstream>
#include <cctype>	//for isdigit
#include <cstring>	//for strlen
#include <sys/stat.h> 	//for stat
#include <stdio.h>	//for perror
#include <err.h>        //for err
#include <locale>
using namespace std;

void trim_spaces(string &s) {
  stringstream trimmer;
  trimmer << s;
  s.clear();
  trimmer >> s;	
}

//convert int to string
void int2str (int n, string& s)
{
  ostringstream convert;
  convert << n;
  s = convert.str();
}

//test a string is number or not
bool is_number(string& s)
{
  if (s.empty()) return false;
  bool firsttime = true;
  for(auto it = s.begin() ; it < s.end(); it++) {
    if (firsttime) {	//skip the '-' for negtive number 
      if (*it == '-') continue;
      firsttime = false;
    }
    if(!isdigit(*it)) return false;
  }
  return true;
}

//test a string is number or not
bool is_number(string s, bool positive)
{
  trim_spaces(s);
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

bool lt0 (string s)
{
  return is_number(s, false);
}

bool gt0(string s)
{
  return is_number(s, true);
}

//check path and output errors, a file or a directory
void check_path(const char *f, bool is_dir)
{
  if (f == NULL)
    errx(1, "[error] file is NULL, abort!");
  if (strlen(f) <= 0)
    errx(1, "[error] file[%s] is not valid, abort!", f);
  
  struct stat statbuf;
  if (stat(f, &statbuf) == -1)
    err(1, "[error] file[%s] cannot be openned", f);
  
  if (is_dir && !S_ISDIR(statbuf.st_mode))	//should be a directory
    errx(1, "[error] file[%s] is not a directory",f);
  else if(!is_dir && S_ISDIR(statbuf.st_mode))	//should be a file
    errx(1, "[error] file[%s] is a directory",f);
}

//split a string
void str_split(string &s, vector<string> &substrs)
{	
  if (s == "")
    return;
  istringstream ss(s);
  while(!ss.eof()) {//remove previous four items
    string x;
    ss >> x;
    substrs.push_back(x);
  }
}

//split a string
//http://www.cplusplus.com/faq/sequences/strings/split/#getline
void str_split(string &s, vector<string> &substrs, char c, bool do_trim)
{
  istringstream ss(s);
  while (!ss.eof()) {
    string x;
    getline(ss, x, c);
    if (do_trim)
      trim_spaces(x);
    substrs.push_back(x);
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

//get time
double get_time_now(struct timeval *tv, struct timezone *tz, string t)
{
  if (gettimeofday(tv, tz) != 0)
    errx(1, "[error] gettimeofday fails!");
  if (t== "second") {
    return (double(tv->tv_sec) + double(tv->tv_usec) / 1000000);
  } else if (t== "ms") {
    return (double(tv->tv_sec) * 1000  + double(tv->tv_usec) / 1000);
  } else if (t== "us") {
    return (double(tv->tv_sec) * 1000000  + double(tv->tv_usec));
  } else {
    cerr << "[ERROR] \"t\" is not supported in get_time_now"<<endl; 
    return -1;
  }
}

double get_time_now(string t)
{
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0)
    errx(1, "[error] gettimeofday fails!");
  if (t== "second") {
    return (double(tv.tv_sec) + double(tv.tv_usec) / 1000000);
  } else if (t== "ms") {
    return (double(tv.tv_sec) * 1000  + double(tv.tv_usec) / 1000);
  } else if (t== "us") {
    return (double(tv.tv_sec) * 1000000  + double(tv.tv_usec));
  } else {
    cerr << "[ERROR] \"t\" is not supported in get_time_now"<<endl; 
    return -1;
  }
}

void get_time_now(struct timeval *tv)
{
  if (gettimeofday(tv, NULL) != 0)
    errx(1, "[error] gettimeofday fails!");
}

string rm_last_dot(string s)
{
  if (s.length() > 1 && s.back() == '.')
    return s.substr(0, (s.length() - 1));
  return s;
}

string str_tolower(string s)
{
  locale loc;
  string t = s;
  for (string::size_type i=0; i<s.length(); i++)
    t[i] = tolower(s[i], loc);
  return t;
}

string fmt_str(string s, string c)
{
  string r = "";
  bool first = true;
  vector<string> v;
  str_split(s, v);
  for (string e : v) {
    if (e.length() == 0 || e == "\n")
      continue;
    if (first)
      first = false;
    else
      r += c;
    r += e;
  }
  return r;
}

void
die(const char *s)
{
	if (s)
		cerr << s;
	exit(1);
}

