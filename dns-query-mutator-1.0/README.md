% dns-query-mutator(1)
% Liang Zhu <liangzhu@isi.edu>
% October 5, 2018

# NAME

dns-query-mutator - change DNS queries in a network trace file

# SYNOPSIS

dns-query-mutator [-i *FORMAT:FILE*] [-o *FORMAT:FILE*] [`--dns-opcode` *PERCENT:NUMBER*]...
                  [-l] [-h] [-v] [-V]

# DESCRIPTION

dns-query-mutator converts DNS query stream for trace replay with optional mutation. 

It deals with three different types of input format:

* network *trace*: any format accepted by libtrace, such as pcap and
  erf file

* plain *text*: a Fsdb file where each line contains data elements
  delimited by spaces.  Each line of the input text file should be
  (time, source ip, query name, query class, query type, protocol).

* customized *raw* binary: a DNS message with prepeded message size.
  The DNS message is defined in dns_msg.proto, and is converted to
  binary by Google's protocol buffer library.

dns-query-mutator supports two kinds of conversion:

* encoding: convert *trace* or *text* to *raw*
* decoding: convert *raw* to *text*

For *trace* input, it supports to modify some fields in DNS header and EDNS:

* opcode
* DNS flags (AA, TC, RD, RA, Z, AD, CD)
* edns: DO, udp_size, extended_rcode, version, Z

# OPTIONS

`-i/--input` *FORMAT:FILE*
:   input file, format and file separated by colon like FORMAT:FILE. Accepted format: *trace* (network trace), *text* (plain text Fsdb), *raw* (customized binary), such as trace:test.pcap, text:test.fsdb, raw:test.raw. use - as FILE to read from stdin or output to stdout.

`-o/--output` *FORMAT:FILE*
:   output file, format and file separated by colon like FORMAT:FILE. Accepted format: *text*, *raw*. Use input type *trace* or *text* with output type *raw* for encoding. Use input type *raw* with output type *text* for decoding. All the other combinations will be ignored.

`--dns-opcode` *PERCENT:NUMBER*
:   set OPCODE to NUMBER in PERCENT% of the queries

`--dns-aa` *PERCENT:NUMBER*
:   set AA bit to NUMBER in PERCENT% of the queries

`--dns-tc` *PERCENT:NUMBER*
:   set TC bit to NUMBER in PERCENT% of the queries

`--dns-rd` *PERCENT:NUMBER*
:   set RD bit to NUMBER in PERCENT% of the queries

`--dns-ra` *PERCENT:NUMBER*
:   set RA bit to NUMBER in PERCENT% of the queries

`--dns-z` *PERCENT:NUMBER*
:   set Z bit to NUMBER in PERCENT% of the queries

`--dns-ad` *PERCENT:NUMBER*
:   set AD bit to NUMBER in PERCENT% of the queries

`--dns-cd` *PERCENT:NUMBER*
:   set CD bit to NUMBER in PERCENT% of the queries

`--edns-do` *PERCENT:NUMBER*
:   set DO bit of EDNS to NUMBER in PERCENT% of the queries. Unsetting DO bit will delete EDNS part from the original packet

`--edns-udp-size` *PERCENT:NUMBER*
:   set EDNS size to NUMBER in PERCENT% of the queries

`--edns-extended-rcode` *PERCENT:NUMBER*
:   set EDNS Rcode to NUMBER in PERCENT% of the queries

`--edns-version` *PERCENT:NUMBER*
:   set EDNS Version to NUMBER in PERCENT% of the queries

`--edns-z` *PERCENT:NUMBER*
:   set EDNS Z bit to NUMBER in PERCENT% of the queries

`-l/--loop`
:   load all the input data into memory and loop input stream to get infinite output

`-h/--help`
:   print this message

`-v/--verbose`
:   verbose log; default is none

`-V/--version`
:   show the program version

# EXAMPLES

1. convert pcap to raw

        ./dns-query-mutator -i trace:t.pcap -o raw:t.raw

2. convert text to raw

        ./dns-query-mutator -i text:t.text -o raw:t.raw

3. convert raw to text

        ./dns-query-mutator -i raw:t.raw -o text:t.text

4. read and write via pipe

        cat t.pcap | ./dns-query-mutator -i trace:- -o raw:- | xz > t.raw.xz
        cat t.fsdb | dbcol time srcip qname qclass qtype protocol | dbfilestripcomments | ./dns-query-mutator -i text:- -o raw:- | xz > t.raw.xz

5. set DO bit in 50% of the input queries

        ./dns-query-mutator -i trace:t.pcap --edns-do 50:1 -o raw:- | xz > t.raw.xz

6. serve as a traffic generator

        ./dns-query-mutator -l -i trace:t.pcap -o raw:- | ./dns-replay-client -s 192.168.1.1:53 -f -i raw:-

# INSTALLATION

To build, type **make**.

It requires the following packages on Fedora:
   ldns-devel
   libtrace-devel
   protobuf-devel

# ALSO SEE

dns-replay-controller(1), dns-replay-client(1), Fsdb(3)

# CHANGES

* 1.0, 2018-10-05: Beta release
