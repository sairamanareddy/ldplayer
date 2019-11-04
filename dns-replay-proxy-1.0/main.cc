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
 */

#include <iostream>
#include <string>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include "proxy.hh"
#include "addr_util.h"
using namespace std;

static bool ValidateProxyType(const char *flagname, const string &p) {
  if (p.empty() || (p != "recursive" && p != "authoritative")) return false;
  return true;
}

static bool ValidateStringNotEmpty(const char *flagname, const string &s) {
  if (s.empty()) return false;
  return true;
}

static bool ValidateAddress(const char *flagname, const string &a) {
  if (a.empty() || (!ipv4_addr(a.c_str()) && !ipv6_addr(a.c_str()))) return false;
  return true;
}

static bool ValidateNumThreads(const char *flagname, int32_t num_threads) {
   if (num_threads < 1) return false;
   return true;
}

DEFINE_string(proxy_type, "recursive", "type of the proxy, \'recursive\' or \'authoritative\'");
DEFINE_validator(proxy_type, &ValidateProxyType);
DEFINE_string(tun_name, "dnstun", "name of the TUN interface");
DEFINE_validator(tun_name, &ValidateStringNotEmpty);
DEFINE_string(auth_addr, "", "IP address of the authoritative server, IPv4 only");
DEFINE_validator(auth_addr, &ValidateAddress);
DEFINE_string(rec_addr, "", "IP address of the recursive server, IPv4 only");
DEFINE_validator(rec_addr, &ValidateAddress);
DEFINE_int32(num_threads, 1, "number of threads, default 1");
DEFINE_validator(num_threads, &ValidateNumThreads);
DEFINE_bool(dry_run, false, "not run proxy and just output debug message");

int main(int argc, char *argv[])
{
  string usage("Sample usage:\n");
  usage += argv[0];
  usage += " --proxy_type=recursive --tun_name=DNSTUN --auth_addr=192.168.1.1 --rec_addr=192.168.1.2 --num_threads=3 --dry_run";

  FLAGS_logtostderr = 1;
  google::InitGoogleLogging(argv[0]);
  gflags::SetUsageMessage(usage);
  gflags::SetVersionString("1.0");
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  VLOG(1) << "TUN name: " << FLAGS_tun_name;
  VLOG(1) << "authoritative address: " << FLAGS_auth_addr;
  VLOG(1) << "recursive address: " << FLAGS_rec_addr;
  VLOG(1) << "proxy_type: " << FLAGS_proxy_type;
  VLOG(1) << "number of threads: " << FLAGS_num_threads;

  DNSReplayProxy dns_replay_proxy(FLAGS_tun_name, FLAGS_auth_addr, FLAGS_rec_addr, FLAGS_proxy_type, FLAGS_num_threads);
  if (!FLAGS_dry_run) {
    dns_replay_proxy.start();
  }
  return 0;
}
