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

#include "global_var.h"
#include "version.h"
#include "str_util.hh"
#include "dns_util.hh"
#include "mutator.hh"

#include <iostream>
#include <cassert>
#include <getopt.h>
#include <unordered_map>
using namespace std;

#define OPT_DNS_OPCODE          1000
#define OPT_DNS_AA              1001
#define OPT_DNS_TC              1002
#define OPT_DNS_RD              1003
#define OPT_DNS_RA              1004
#define OPT_DNS_Z               1005
#define OPT_DNS_AD              1006
#define OPT_DNS_CD              1007
#define OPT_EDNS_DO             1008
#define OPT_EDNS_UDP_SIZE       1009
#define OPT_EDNS_EXTENDED_RCODE 1010
#define OPT_EDNS_VERSION        1011
#define OPT_EDNS_Z              1012

bool verbose_log = false;

void usage(const char *comm) {
  cerr << " Usage:\n " <<
    comm << " [-i FORMAT:FILE] [-o FORMAT:FILE]\n"
    "         [-l] [-h] [-v] [-V]\n"
    " -i/--input FORMAT:FILE    input file, use '-' as FILE to read from stdin\n"
    "                           format and file separated by colon like FORMAT:PATH\n"
    "                           accepted format: trace, text, raw\n"
    "                           e.g. trace:test.pcap, text:test.fsdb, raw:test.raw\n"
    " -o/--output FORMAT:FILE   output file, use '-' as FILE to write to stdout\n"
    "                           format and file separated by colon like FORMAT:PATH\n"
    "                           accepted format: text, raw\n"
    "                           use input type trace and text with output type raw for 'encoding'\n"
    "                           use input type raw with output type text for 'decoding'\n"
    "                           all the other combinations will be ignored\n"
    " --dns-opcode N:X          set OPCODE to X in N% of the queries\n"
    " --dns-aa N:X              set AA bit to X in N% of the queries\n"
    " --dns-tc N:X              set TC bit to X in N% of the queries\n"
    " --dns-rd N:X              set RD bit to X in N% of the queries\n"
    " --dns-ra N:X              set RA bit to X in N% of the queries\n"
    " --dns-z  N:X              set Z bit to X in N% of the queries\n"
    " --dns-ad N:X              set AD bit to X in N% of the queries\n"
    " --dns-cd N:X              set CD bit to X in N% of the queries\n"
    " --edns-do N:X             set DO bit of EDNS to X in N% of the queries\n"
    "                           unset DO bit deletes EDNS part from the original packet\n"
    " --edns-udp-size N:X       ...\n"
    " --edns-extended-rcode N:X ...\n"
    " --edns-version N:X        ...\n"
    " --edns-z N:X              ...\n"
    " -l/--loop                 load all the input data into memory and\n"
    "                           loop input stream to get infinite output\n"
    " -h/--help                 print this message\n"
    " -v/--verbose              verbose log; default is none\n"
    " -V/--version              show the program version\n"
    "\n(" << VERSION << ")\n";
  exit(1);
}

int main(int argc, char *argv[])
{
  const char *comm = argv[0];
  if (argc == 1) usage(comm);

  string input_file, input_format;
  string output_file, output_format;
  bool loop = false;
  string tmp;
  int opt = -1;

  unordered_map<unsigned int, string> m;
  
  struct option long_options[] = {
    {"help",          0, NULL, 'h'},
    {"input",         1, NULL, 'i'},
    {"loop",          0, NULL, 'l'},
    {"output",        1, NULL, 'o'},
    {"verbose",       0, NULL, 'v'},
    {"version",       0, NULL, 'V'},
    {"dns-opcode",    1, NULL, OPT_DNS_OPCODE},
    {"dns-aa",        1, NULL, OPT_DNS_AA},
    {"dns-tc",        1, NULL, OPT_DNS_TC},
    {"dns-rd",        1, NULL, OPT_DNS_RD},
    {"dns-ra",        1, NULL, OPT_DNS_RA},
    {"dns-z",         1, NULL, OPT_DNS_Z},
    {"dns-ad",        1, NULL, OPT_DNS_AD},
    {"dns-cd",        1, NULL, OPT_DNS_CD},
    {"edns-do",       1, NULL, OPT_EDNS_DO},
    {"edns-udp-size", 1, NULL, OPT_EDNS_UDP_SIZE},
    {"edns-extended-rcode", 1, NULL, OPT_EDNS_EXTENDED_RCODE},
    {"edns-version",  1, NULL, OPT_EDNS_VERSION},
    {"edns-z",        1, NULL, OPT_EDNS_Z}
  };
  
  while ((opt = getopt_long(argc, argv, "i:o:hlvV", long_options, NULL)) != EOF) {
    tmp.clear();
    if (optarg) tmp = optarg;
    switch(opt) {
    case 'i':
      str_split(tmp, input_format, input_file, ':');
      break;
    case 'l':
      loop = true;
      break;
    case 'o':
      str_split(tmp, output_format, output_file, ':');
      break;
    case 'v':
      verbose_log = true;
      break;
    case 'V':
      errx(0, VERSION);
      break;
    case OPT_DNS_OPCODE:
      m[DNS_OPT_MAP["dns-opcode"]] = tmp;
      break;
    case OPT_DNS_AA:
      m[DNS_OPT_MAP["dns-aa"]] = tmp;
      break;
    case OPT_DNS_TC:
      m[DNS_OPT_MAP["dns-tc"]] = tmp;
      break;
    case OPT_DNS_RD:
      m[DNS_OPT_MAP["dns-rd"]] = tmp;
      break;
    case OPT_DNS_RA:
      m[DNS_OPT_MAP["dns-ra"]] = tmp;
      break;
    case OPT_DNS_Z:
      m[DNS_OPT_MAP["dns-z"]] = tmp;
      break;
    case OPT_DNS_AD:
      m[DNS_OPT_MAP["dns-ad"]] = tmp;
      break;
    case OPT_DNS_CD:
      m[DNS_OPT_MAP["dns-cd"]] = tmp;
      break;
    case OPT_EDNS_DO:
      m[DNS_OPT_MAP["edns-do"]] = tmp;
      break;
    case OPT_EDNS_UDP_SIZE:
      m[DNS_OPT_MAP["edns-udp-size"]] = tmp;
      break;
    case OPT_EDNS_EXTENDED_RCODE:
      m[DNS_OPT_MAP["edns-extended-code"]] = tmp;
      break;
    case OPT_EDNS_VERSION:
      m[DNS_OPT_MAP["edns-version"]] = tmp;
      break;
    case OPT_EDNS_Z:
      m[DNS_OPT_MAP["edns-z"]] = tmp;
      break;
    default:
      usage(comm);
    }
  }

  //check input error
  if (input_format.empty() || input_file.empty() || !str_set(input_format, {"trace", "text", "raw"}, "input format"))
    errx(1, "input is invalid");
  if (output_format.empty() || output_file.empty() || !str_set(output_format, {"text", "raw"}, "output format"))
    errx(1, "output is invalid");
  if (output_format == "raw") { //encode
    if (input_format == "raw") {
      warnx("ignore raw -> raw, exit!");
      return 0;
    }
  } else if (output_format == "text") {//decode
    if (str_set(input_format, {"text", "trace"}, "")) {
      warnx("ignore trace/text -> text, exit!");
      return 0;
    }
  } else
    errx(1, "output format is invalid");//this should not execute, but just in case

  if (verbose_log) {
    cerr << "# input format:  " << input_format << endl
	 << "# input file:    " << input_file << endl
	 << "# output format: " << output_format << endl
	 << "# output file:   " << output_file << endl
	 << "# loop is " << (loop?"":"not ") << "set" << endl;
  }

  Mutator mut(input_file, input_format, output_file, output_format, loop, m);
  mut.start();
  return 0;
}
