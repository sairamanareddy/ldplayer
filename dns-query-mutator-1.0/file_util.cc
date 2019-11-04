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

#include "file_util.hh"
#include <err.h>        //for warnx
#include <iostream>
#include <cstring>      //for strlen
#include <sys/stat.h>   //for stat
using namespace std;

// NOTE: all the functions should not exit, let the caller make
// decisions based on return values

//check path and output errors, a file or a directory
bool check_path(const char *f, bool is_dir)
{
  if (f == NULL) {
    warnx("file is NULL");
    return false;
  }
  if (strlen(f) <= 0) {
    warnx("file [%s] is not valid", f);
    return false;
  }

  struct stat statbuf;
  if (stat(f, &statbuf) == -1) {
    warn("file [%s] cannot be openned", f);
    return false;
  }

  if (is_dir && !S_ISDIR(statbuf.st_mode)) {      //should be a directory
    warnx("file [%s] is not a directory", f);
    return false;
  } else if(!is_dir && S_ISDIR(statbuf.st_mode))  {//should be a file
    warnx("file [%s] is a directory", f);
    return false;
  }

  return true;
}
