#!/usr/bin/env bash

# This is a set of variables that are used in generate_zone_files.sh.
# Please change them accordingly based on your operating system.

# for logging purpose
id=`date +%s`
log_dir="./log"

# A file that contains input queries. A line in the file should look
# like "google.com A" (a single space between query name and query
# type)
input_query="./sample_data/query.txt"

# output directory for tcpdump output
trace_output="./sample_data/tcpdump_output"

# -Z option for tcpdump command line
tcpdump_user=$USER

# the name of the interface for tcpdump, "any": all interfaces to
# capture all the data
interface_name="any"   

# the process name (BIND) for the server used to capture traces; use
# "named" for Fedora and "bind9" for Ubuntu
server_process="named"

# the address of the server used to capture traces
server_address=""

# the path to dnsanon program
dnsanon=""

# the output directory for dnsanon
dnsanon_output="./sample_data/dnsanon_output"

# the following are options for ./zone/bin/convert. For more
# information, check out ./zone/bin/convert --help
zone_output="./sample_data/zone_output/zones"        # --output-directory
zone_path="/etc/bind/zones"                          # --zone-path
config_output="./sample_data/zone_output/named.conf" # --config
root_hint="./named.root"                             # --root-hint
exp_server_address="10.1.1.4"                        # --authoritative-address 

