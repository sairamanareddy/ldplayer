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

#ifndef READER_HH
#define READER_HH

#include "input_source.hh"
#include <string>

#define POSTMAN_FLAG "POSTMAN-READY"
#define UNBLOCK_FLAG "UNBLOCK"


// reader reads input file and sends the data to postman by unix
// socket reader and postman should run at the same machine

class Reader {
public:
  Reader(std::string, std::string, int);
  ~Reader();
  bool unblock(const char *);
  void read_input(int);

private:
  int skt;
  int trace_limit;
  pid_t my_pid;

  InputSource *ins;

  std::string input_file;
  std::string input_format;

  void clean_up();
  void log_err(const char *);
};

#endif //READER_HH
