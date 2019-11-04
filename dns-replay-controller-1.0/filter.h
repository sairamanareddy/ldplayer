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

#ifndef FILTER_H
#define FILTER_H

#include <string>
#include <fstream>
#include <vector>

const int kFilterTotalWeight = 100;

class Filter {
public:
  Filter();
  Filter(std::string, int);
  Filter(const Filter&);
  ~Filter();
  bool Valid();
  void Print();
  void Copy(const Filter&);
  void Set(std::string, int);

  int GetNumClients() const;
  int GetWeight(int) const;
  int GetClientIndex(int) const;

  std::string GetFilterInput() const;
  std::string GetWeightStr() const;

private:
  int num_clients;
  int total_weight;

  std::string filter_input;
  std::ifstream ifs;

  std::vector<int> weight;       //size is num_clients
  std::vector<int> client_index; //size is kFilterTotalWeight

  bool GetWeightByFile();
  bool GetWeightByString();
  bool ValidateWeight();

  void Init();
};

#endif //FILTER_H
