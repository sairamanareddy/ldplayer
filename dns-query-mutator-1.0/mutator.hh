/*
 * Copyright (C) 2017-2018 by the University of Southern California
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

#ifndef MUTATOR_HH
#define MUTATOR_HH

#include <string>
#include <fstream>
#include <vector>
#include <utility>
#include <cstdint>
#include <unordered_map>
#include "input_stream.hh"

class Mutator{
public:
  Mutator(std::string, std::string, std::string, std::string, bool,
	  std::unordered_map<unsigned int, std::string> &);
  Mutator(std::string, std::string, std::string, std::string, bool,
	  std::unordered_map<unsigned int, std::pair<int, uint32_t>> &);
  ~Mutator();

  void start();

private:
  bool infinite;
  bool is_stdout;
  
  std::string input_file;
  std::string input_format;
  std::string output_file;
  unsigned int output_format;
  std::ofstream ofs;

  std::unordered_map<unsigned int, std::pair<int, uint32_t>> opt;
  std::vector<trace_replay::DNSMsg *> warehouse;

  InputStream *ins = NULL;

  void init(std::string, std::string, std::string, std::string, bool,
	    std::unordered_map<unsigned int, std::pair<int, uint32_t>> &);
  void mutate_query(trace_replay::DNSMsg *);
  void write_output(trace_replay::DNSMsg *);
};

#endif //MUTATOR_HH
