% dns-replay-controller(1)
% Liang Zhu <liangzhu@isi.edu>
% October 5, 2018

# NAME

dns-replay-controller - read DNS query input and distribute to replay clients

# SYNOPSIS

dns-replay-controller [`--input` *FORMAT:PATH*] [`--output` *OUPUT*]
		      [`--address` *ADDRESS*] [`--port` *PORT*]
		      [`--num_clients` *NUMBER*] [`--trace_limit` *SECONDS*]
		      [`--filter` *FILTER*] [`--version`] [`--help`] [`--dry_run`]

# DESCRIPTION

dns-replay-controller reads DNS query stream and distributes queries
to replay clients (dns-replay-client).

It deals with three different types of input format:

* network *trace*: any format accepted by libtrace, such as pcap and
  erf file

* plain *text*: a Fsdb file where each line contains data elements
  delimited by spaces.  Each line of the input text file should be
  (time, source ip, query name, query class, query type, protocol).

* customized *raw* binary: a DNS message with prepended message size.
  The DNS message is defined in dns_msg.proto, and is converted to
  binary by Google's protocol buffer library.

dns-query-mutator can convert network trace files to raw files.

It is recommended to use *raw* input files when the input query rate
is high, in order to achieve the actual query rate.

By default, dns-replay-controller loads all the input traces into
memory.  Option `--trace_limit` can preload limited seconds of traces
to control RAM usage.

# OPTIONS

`--input` *FORMAT:PATH*
:   input stream, format and file separated by colon like FORMAT:FILE.
    Accepted format: 'trace' (network trace), 'text' (plain text Fsdb), 'raw' (customized binary).
    Use '-' as FILE to read from standard input.

`--output` *FILE*
:   specify output file which contains the output data from clients, '-' for standard output

`--address` *ADDRESS*
:   address to listen on

`--port` *PORT*
:   port number to listen on

`--num_clients` *NUMBER*
:   the total number of clients allowed

`--trace_limit` *SECONDS*
:   preload seconds of trace, used to control memory consumption
    default is none (0 or negative integer): read all in memory

`--filter` *FILTER*
:   colon separated numbers to indicate different portions of queries
    to different clients. E.g. string "10,30,60" means 10%, 30% and 60%
    queries to 1st, 2nd and 3rd clients. Or a file that provides such
    information.

`--help`
:   print help message. For short message, try "--helpmatch main"

`--version`
:   show the program version

`--dry_tun`
:   run the program without create network connections, useful for debugging input

# EXAMPLES

1. running at port 10053 on 192.168.1.100 and allow 20 clients

        ./dns-replay-controller --input raw:t.raw --address 192.168.1.100 --port 10053 -n 20

2. read from stdin

        cat t.raw | ./dns-replay-controller --input raw:- --address 192.168.1.100 --port 10053 -n 20

3. only preload 15-second trace to limit memory usage

        ./dns-replay-controller --input raw:t.raw --address 192.168.1.100 --port 10053 -n 20 --trace_limit 15

4. show more log message

        ./dns-replay-controller --input raw:t.raw --address 192.168.1.100 --port 10053 -n 20 --trace_limit 15 --v=2

5. distribute 30%, 50% and 20% of the input queries to 1st, 2nd and 3rd clients.

        ./dns-replay-controller --input raw:t.raw --address 192.168.1.100 --port 10053 -n 3 --trace_limit 15 --filter 30,50,20

# INSTALLATION

To build, type *make*.

It requires the following packages on Fedora:
   ldns-devel
   libtrace-devel
   libevent-devel
   protobuf-devel
   glog-devel
   gflags-devel

# ALSO SEE

dns-replay-client(1), dns-query-mutator(1), Fsdb(3)

# CHANGES
* 0.1, 2016-10-22: initial release
* 0.2, 2018-03-19: add `--filter` option and use gflags and glog
* 1.0, 2018-10-05: improve documentation and Beta release
