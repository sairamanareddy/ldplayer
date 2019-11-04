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

#include "reader.h"
#include <iostream>
#include <cstring>
#include <signal.h>
#include <err.h>
#include <cstdlib>
#include <sys/types.h>
#include <unistd.h>

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include "dns_util.hh"
#include "utility.hh"

#include <glog/logging.h>

using namespace std;

#define MAX_BUF_SIZE    4096
#define MIN_BUF_SIZE    64
#define SLEEP_TIME      1.0

static void signal_cb(evutil_socket_t sig, short what, void *arg)
{
  assert(what & EV_SIGNAL);
  struct event_base *base = (struct event_base *)arg;
  struct timeval delay = {2, 0};
  cerr << "[" << getpid() <<"]" << ": Signal (" << sig
       << ") received, exit in two seconds!\n";
  event_base_loopexit(base, &delay);
}

Reader::Reader(string fn, string ft, int s)
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  my_pid = getpid();
  input_file = fn;
  input_format = ft;
  skt = s;
  ins = new InputSource(fn, ft);
  assert(ins);
}

Reader::~Reader()
{
  clean_up();
}

/*
  read input file
*/
void Reader::read_input(int l)
{
  trace_limit = l;
  VLOG(3) << "[" << my_pid << "] start to read and distribute input data with trace_limit=" << trace_limit;
  trace_replay::DNSMsg *msg = NULL;
  string raw;
  string msg_str;
  double real_start_ts = -1.0;
  double trace_start_ts = -1.0;
  while((msg = ins->get()) != NULL) {
    assert(msg);
    raw = msg->raw();
    if (raw.size() == 0) {//not a valid query
      delete msg;
      msg = NULL;
      continue;
    }
    //print_dns_pkt((uint8_t*)raw.data(), raw.size());
    VLOG(3) << "[" << my_pid << "] read from input raw size=" << raw.size();

    if (trace_limit > 0) {
      double trace_current_ts = double(msg->seconds()) + double(msg->microseconds())/1000000.0;
      //warnx("trace_limit: trace_current_ts: %.6f", trace_current_ts);
      if (trace_start_ts < 0) {
        trace_start_ts = trace_current_ts;
        real_start_ts = get_time_now("second");
      } else {
	while (
               (trace_current_ts - trace_start_ts)     //trace_ts_diff
               -
               (get_time_now("second") - real_start_ts)//real_ts_diff
               >
               (trace_limit + double(SLEEP_TIME))      //limit_ts
               ) {
	  warnx("go to sleep: %.6f", get_time_now("second"));
          sleep(SLEEP_TIME); //wait for SLEEP_TIME
        }
      }
    }
    
    if (!(msg->SerializeToString(&msg_str)))
      log_err("serialize message fails");
    assert(msg_str.size() > 0);

    //write length and then the data
    uint32_t sz = msg_str.size();
    sz = htonl(sz);
    if (-1 == write(skt, &sz, sizeof(sz)))
      log_err("fail to write socket");
    if (-1 == write(skt, msg_str.data(), msg_str.size()))
      log_err("fail to write socket");

    //clean up
    raw.clear();
    msg_str.clear();
    delete msg;
  }
  VLOG(3) << "[" << my_pid << "] end of reading input";
}

/*
  event callback function of postman socket
*/
void skt_event_cb(struct bufferevent *bev, short events, void *ctx)
{
  if (events & BEV_EVENT_ERROR) {
    int e = bufferevent_socket_get_dns_error(bev);
    if (e)
      err(1, "event error: %s\n", evutil_gai_strerror(e));
  }
}

/*
  read callback function of postman socket
*/
void skt_read_cb(struct bufferevent *bev, void *ctx)
{
  struct event_base *base = bufferevent_get_base(bev);
  string *flag = (string *)ctx;
  
  char buf[MIN_BUF_SIZE];
  memset(buf, 0, sizeof(buf));

  struct evbuffer *input_buffer = bufferevent_get_input(bev);
  int n = evbuffer_remove(input_buffer, buf, sizeof(buf));

  if (n > 0) {
    if (strcmp(buf, flag->c_str()) == 0) {
      warnx("unblock");
      *flag = UNBLOCK_FLAG;
      event_base_loopexit(base, NULL);
    } else {
      warnx("not match flag, still block");
    }    
  }
}

/*
  blocking for synchronization
*/
bool Reader::unblock(const char *flag)
{
  VLOG(3) << "[" << my_pid << "] starts";
  if (skt <= 0)
    errx(1, "[error] socket is not set");
  string flag_str = flag;
  //create event base
  struct event_base *base = event_base_new();
  if (!base) err(1, "cannot open event base");

  //set up signal event
  struct event *sigint_event = evsignal_new(base, SIGINT, signal_cb, (void *)base);
  if (event_add(sigint_event, NULL) < 0) err(1, "cannot add sigint event");
  struct event *sigterm_event = evsignal_new(base, SIGTERM, signal_cb, (void *)base);
  if (event_add(sigterm_event, NULL) < 0) err(1, "cannot add sigterm event");

  //set up input socket event
  //DO NOT BEV_OPT_CLOSE_ON_FREE
  struct bufferevent *skt_bev = bufferevent_socket_new(base, skt, 0);
  bufferevent_setcb(skt_bev, skt_read_cb, NULL, skt_event_cb, &flag_str);
  bufferevent_enable(skt_bev, EV_READ);

  event_base_dispatch(base);
  //clean up
  bufferevent_free(skt_bev);
  event_free(sigint_event);
  event_free(sigterm_event);
  event_base_free(base);

  return flag_str == UNBLOCK_FLAG;
}

/*
  clean up
*/
void Reader::clean_up()
{
  if (ins != NULL)
    delete ins;
}

void Reader::log_err(const char *s)
{
  clean_up();
  err(1, "[%d] [error] %s, abort!", my_pid, s);
}
