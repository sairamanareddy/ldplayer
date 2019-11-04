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

#include "safe_queue.hh"
#include <iostream>
#include <tuple>
using namespace std;

template<typename T>
safe_queue<T>::safe_queue(int m)
{
  max_len = m;
}

template<typename T>
safe_queue<T>::~safe_queue()
{
  /*unique_lock<mutex> lk (mtx);
  T item;
  while (!q.empty()) {
    item = q.front();
    q.pop();
    //if (is_pointer<T>::value)
    //  delete item;
  }*/
}

template<typename T>
void safe_queue<T>::push(T x)
{
  unique_lock<mutex> lk (mtx);
  while(q.size() >= max_len)
    cv.wait(lk);
  q.push(x);
  lk.unlock();
  cv.notify_one();
}

template<typename T>
T safe_queue<T>::pop()
{
  unique_lock<mutex> lk (mtx);
  while (q.empty())
    cv.wait(lk);
  auto item = q.front();
  q.pop();
  lk.unlock();
  cv.notify_one();
  return item;
}

template<typename T>
void safe_queue<T>::print()
{
  /*  unique_lock<mutex> lk (mtx);
  int i = 0;
  cout << "start print; len[" << max_len << "]" << endl;
  while (!q.empty()) {
    cout << i << ":" << q.front() << endl;
    q.pop();
    i += 1; 
  }*/
}

template<typename T>
bool safe_queue<T>::empty()
{
  unique_lock<mutex> lk (mtx);
  return q.empty();
}

template<typename T>
unsigned int safe_queue<T>::get_max_len()
{
  return max_len;
}

//fix undefined reference to `safe_queue<int>::safe_queue(int)'
template class safe_queue<tuple<uint8_t *, int> >;

