/*                                                                                   
 * Copyright (C) 2015-2018 by the University of Southern California
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

#ifndef SAFE_QUEUE_HH
#define SAFE_QUEUE_HH

#include <mutex>
#include <queue>
#include <condition_variable>

template <typename T>
class safe_queue {
public:
  safe_queue(int);
  ~safe_queue();
  T pop();
  void push(T x);
  void print();
  bool empty();
  unsigned int get_max_len();

private:
  unsigned int max_len;
  std::queue<T> q;
  std::mutex mtx;
  std::condition_variable cv;
};

#endif //SAFE_QUEUE_HH
