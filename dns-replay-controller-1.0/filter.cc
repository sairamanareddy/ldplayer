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


#include "filter.h"
#include "utility.hh"
#include <iostream>
#include <glog/logging.h>
using namespace std;

Filter::Filter(): num_clients(0),total_weight(0),filter_input("")
{

}

Filter::Filter(string input, int num): num_clients(num),total_weight(0),filter_input(input)
{
  Init();
}

Filter::Filter(const Filter &f)
{
  Filter(f.GetFilterInput(), f.GetNumClients());
}

Filter::~Filter()
{
  if (ifs.is_open()) {
    ifs.close();
  }
}

void Filter::Init()
{
  if (num_clients < 1) {
    LOG(ERROR) << "num_clients=" << num_clients;
  }
  weight.clear();
  weight.resize(num_clients, -1);

  client_index.clear();
  client_index.resize(kFilterTotalWeight, -1);
  
  if (filter_input.empty()) {
    LOG(ERROR) << "cannot get weight because filter input is empty";
  } else if (!GetWeightByFile() && !GetWeightByString()) {
    LOG(ERROR) << "failed to get filter";
  }

  // fill client index map
  // e.g. weight = [20, 30, 50]
  // => client_index[0] -> client_index[19]: 0
  //    client_index[20] -> client_index[49]: 1
  //    client_index[50] -> client_index[99]: 2
  unsigned int start = 0, end = 0;
  for (unsigned int i=0; i<weight.size(); i++) {
    end = start + weight[i];
    for (unsigned int j=start; j<client_index.size() && j < end; j++) {
      client_index[j] = i;
    }
    start = end;
  }
}

int Filter::GetNumClients() const
{
  return num_clients;
}

string Filter::GetFilterInput() const
{
  return filter_input;
}

string Filter::GetWeightStr() const
{
  string s;
  for (unsigned int i=0; i<weight.size(); i++) {
    if (i != 0) s += ",";
    s += to_string(weight[i]);
  }
  return s;
}


bool Filter::GetWeightByFile()
{
  string line;
  int i = 0;
  ifs.open(filter_input);
  if (!ifs.is_open()) {
    LOG(WARNING) << "cannot get filter from file";
    return false;
  }
  try {
    while (getline(ifs, line)) {
      if (i >= (int)weight.size()) break;
      weight[i++] = stoi(line);
      total_weight += weight[i-1];
      line.clear();
    }
  } catch (const std::exception& e) {
    LOG(ERROR) << "invalid input: " << e.what();
    return false;
  }
  VLOG(3) << "Successfully get weight map from file";
  return true;
}

bool Filter::GetWeightByString()
{
  if (filter_input.empty()) {
    LOG(WARNING) << "cannot get filter from string";
    return false;
  }
  vector<string> w;
  str_split(filter_input, w, ',', true);
  try {
    for (unsigned int i=0; i<w.size() && i<weight.size(); i++) {
      weight[i] = stoi(w[i]);
      total_weight += weight[i];
    }
  } catch (const std::exception& e) {
    LOG(ERROR) << "invalid input: " << e.what();
    return false;
  }  
  VLOG(3) << "Successfully get weight map from string";
  return true;
}

bool Filter::ValidateWeight()
{
  int n = 0;
  for (auto w : weight) {
    if (w == -1) {
      n++;
    }
  }
  if (n != 0) {
    LOG(WARNING) << n << " clients' weight is not set";
    return false;
  }
  if (total_weight != kFilterTotalWeight) {
    LOG(WARNING) << "total_weight[" << total_weight << "] != " << kFilterTotalWeight;
    return false;
  }
  return true;
}

void Filter::Print()
{
  VLOG(3) << "print weight map:";
  for (unsigned int i=0; i<weight.size(); i++) {
    LOG(INFO) << i << " => " << weight[i];
  }
  VLOG(3) << "print client_index map count:";
  int c = 0, p = 0;
  for (unsigned int i=0; i<client_index.size(); i++) {
    if (i != 0 && client_index[i] != p) {
      LOG(INFO) << p << ": " << c;
      c = 0;
    }
    c++;
    p = client_index[i];
    if (i == client_index.size()-1) {
      LOG(INFO) << p << ": " << c;
    }
  }
}

void Filter::Copy(const Filter &f)
{
  VLOG(3) << "start to copy filter";
  Print();
  num_clients = f.GetNumClients();
  filter_input = f.GetFilterInput();
  total_weight = 0;
  Init();
  VLOG(3) << "after copy filter";
  Print();
}

bool Filter::Valid()
{
  return ValidateWeight();
}

int Filter::GetWeight(int i) const
{
  if (i < 0 || i >= (int)weight.size()) {
    return -1;
  }
  return weight[i];
}

int Filter::GetClientIndex(int i) const
{
  if (i < 0 || i >= kFilterTotalWeight) {
    return -1;
  }
  return client_index[i];
}

void Filter::Set(string input, int num)
{
  num_clients = num;
  filter_input = input;
  total_weight = 0;
  if (num_clients > 0 && !filter_input.empty()) {
    Init();
  }
}
