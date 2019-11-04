% dns-replay-client(1)
% Liang Zhu <liangzhu@isi.edu>
% October 5, 2018

# NAME

dns-replay-client - replay DNS queries with accurate timing

# SYNOPSIS

dns-replay-client [-i *FORMAT:PATH*] [-o *OUTPUT*] [-s *IP:PORT*] [-r *IP:PORT*] [-n *NUMBER*]
                  [-c *TYPE*] [-t *TIMEOUT*] [-l *SECONDS*] [-u] [-d] [-f] [-v] [-V] [-h]

# DESCRIPTION

dns-replay-client replays DNS queries against a real DNS server with
correct timing. Optionally it can log the latency for each query, or
the timing for each query and response.  Multiple instances of
dns-replay-client can work coordinately to replay large query stream
with dns-replay-controller.

It deals with three different types of input format:

* network *trace*: any format accepted by libtrace, such as pcap and
  erf file

* plain *text*: a Fsdb file where each line contains data elements
  delimited by spaces.  Each line of the input text file should be
  (time, source ip, query name, query class, query type, protocol).

* customized *raw* binary: a DNS message with prepeded message size.
  The DNS message is defined in dns_msg.proto, and is converted to
  binary by Google's protocol buffer library.

dns-query-mutator can convert network trace files to raw files.

It is recommended to use *raw* input files when the input query rate
is high, in order to achieve the actual query rate.

By default, dns-replay-client creates multiple processes to utilize
parallelism of multi-core CPU settings.

By default, dns-replay-client loads all the input trace into memory.
Option `-l/--limit` can preload limited seconds of traces to control RAM
usage.

# OPTIONS

`-i/--input` *FORMAT:FILE*
:   input stream, format and file separated by colon like FORMAT:FILE.
    Accepted format: **trace** (network trace), **text** (plain text Fsdb), **raw** (customized binary).
    Use - as FILE to read from standard input.
    It is required without option -d.

`-o/--output` *FORMAT:FILE*
:   *optional* output file, format and file separated by colon like FORMAT:FILE.
    Accepted format: **latency** (output the latency of each query), **timing** (output the timing of each query and response).
    Use - as FILE to write to stdout.

`-s/--server` *IP:PORT*
:   server address and port, separated by colon, e.g. 192.168.1.1:53
			   
`-r/--controller` *IP:PORT*
:   address and port to receive message from controller, e.g. 192.168.1.100:10053,
    required in distributed mode (-d)

`-n/--num-workers` *NUMBER*
:   specify number of worker processes. The default is the number of CPU cores.

`-c/--connections` *TYPE*
:   specify the type connection that the queries are send over.
    Accepted options: udp/tcp/tls/adaptive, adaptive: query with protocol in input stream.
    (Note that tls has not been implemented yet.)

`-t/--timeout` *TIMEOUT*
:   specify the timeout for tcp/tls connections, default is 30 seconds

`-l/--limit` *SECONDS*
:   preload seconds of trace, used to control memory consumption
    default is none (or any negative integer): read all in memory
    option '-f' set this to none automatically

`-u/--unify-udp`
:   each worker uses one socket for all the UDP queries.
    By default it uses different sockets for different source IP.	   

`-d/--distribute`
:   distributed mode with reading input stream from controller.
    This requires dns-replay-controller running separately.

`-f/--fast`
:   the fastest query replay rate:
    send input queries immediately without setup timer

`-h/--help`
:   print help message

`-v/--verbose`
:   verbose log; default is none

`-V/--version`
:   show the program version

# EXAMPLES

Assume there are input files:

test.pcap:

    contains one UDP query and one TCP query for "www.isi.edu A".

test.fsdb:

    #fsdb time src_ip qname qclass qtype protocol
    1427330638.079111 192.168.1.1 www.isi.edu IN A udp
    1427330638.079222 192.168.1.2 www.isi.edu IN A tcp

test.raw:

    ./dns-query-mutator -i trace:test.pcap -o raw:test.raw

1. replay queries over UDP and output latency to file test.txt

        ./dns-replay-client -i text:test.fsdb -s 192.168.1.200:53 -c udp -o latency:test.txt
        or use pcap input with -i trace:test.pcap

2. replay queries over TCP and output the timing of reach query and response to stdout

        ./dns-replay-client -i text:test.fsdb -s 192.168.1.200:53 -c tcp -o timing:-

3. replay queries over the protocol given in the input

        ./dns-replay-client -i text:test.fsdb -s 192.168.1.200:53 -c adaptive -o timing:-

4. preload 15-second trace to control ram usage

        ./dns-replay-client -l 15 -i raw:test.raw -s 192.168.1.200:53 -c adaptive -o timing:-

5. use standard input

        cat test.fsdb | ./dns-replay-client -i text:- -s 192.168.1.200:53 -c adaptive -o latency:-
        cat test.pcap | ./dns-replay-client -i trace:- -s 192.168.1.200:53 -c adaptive -o latency:-
        cat test.raw | ./dns-replay-client -i raw:- -s 192.168.1.200:53 -c adaptive -o latency:-

6. run in distributed mode

        assume dns-replay-controller is running at port 10053 on 192.168.1.100
        ./dns-replay-client -d -s 192.168.1.200:53 -c adaptive -o timing:- -r 192.168.1.100:10053

# INSTALLATION

To build, type *make*.

It requires the following packages on Fedora:
   ldns-devel
   libtrace-devel
   libevent-devel
   protobuf-devel

# ALSO SEE

dns-replay-controller(1), dns-query-mutator(1), Fsdb(3)

# CHANGES
* 0.1, 2016-10-22: initial release
* 1.0, 2018-10-05: Bug fixes and Beta release
