#!/usr/bin/env bash

# the source code for dns-replay-proxy
px_src=""
px_exe="dns-replay-proxy"

# recursive server
rec_name=""
rec_addr="10.1.1.3"
rec_interface="eth4"
rec_port="53"

# authoritative server
auth_name=""
auth_addr="10.1.1.4"
auth_interface="eth5"
auth_port="53"

# used in exp_setup_route_proxy.sh
table=12
mark=12
tun="dnstun"
routing_script="./route_setup.sh"
