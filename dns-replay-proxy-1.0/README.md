% dns-replay-proxy(1)
% Liang Zhu <liangzhu@isi.edu>
% October 5, 2018

# NAME

dns-replay-proxy - a proxy to emulate DNS hierarchy

# SYNOPSIS

dns-replay-proxy [`--auth_addr` *IP*] [`--rec_addr` *IP*] [`--num_threads` *NUMBER*]
		 [`--proxy_type` *TYPE*] [`--tun_name` *NAME*] [`--dry_run`]

# DESCRIPTION

dns-replay-proxy manipulates packet addresses to emulate DNS hierarchy
in LDplayer.  Specifically, dns-replay-proxy reads packets from the
given tunnel interface, rewrites the source and destination addresses,
recomputes the check-sum and sends out the modified packets.

dns-replay-proxy has two different modes, **recursive** (default) or
**authoritative**, specified by `--proxy_type`. It should run at the
same machine of either a recursive server (`--proxy_type recursive`)
or a authoritative server (`--proxy_type authoritative`).  The
authoritative server should run with split-horizon DNS, selecting zone
by matching query source IP addresses.

It is recommended to use multiple threads (`--num_threads`) for fast
packet processing.

There are a few steps before running dns-replay-proxy.

(You might want to use scripts in LDplayer/dns-route-setup for the
following setup.)

First, you need to create two network tunnel interfaces at the
recursive and the authoritative servers respectively, for example:

    sudo ip tuntap add dev $TUN mode tun
    sudo ifconfig $TUN 172.16.0.1/16 up

Second, you need to setup port-based routing. All DNS queries out of
the recursive server and all responses out of the authoritative server
must be routed to the network tunnel interfaces, for example:

    at recursive server:
    sudo iptables -A OUTPUT -t mangle -p udp -s $SERVER_IP --dport 53 -j MARK --set-mark $MARK
    sudo iptables -A OUTPUT -t mangle -p tcp -s $SERVER_IP --dport 53 -j MARK --set-mark $MARK

    at authoritative server:
    sudo iptables -A OUTPUT -t mangle -p udp -s $SERVER_IP --sport 53 -j MARK --set-mark $MARK
    sudo iptables -A OUTPUT -t mangle -p tcp -s $SERVER_IP --sport 53 -j MARK --set-mark $MARK

    at both servers:
    sudo ip route add default via 172.16.0.1 dev $TUN table $TABLE
    sudo ip rule add from $SERVER_IP fwmark $MARK table $TABLE

The message flow would look like the following:

                       +-----------+                     +---------------+
                       | Recursive |-------------------->| Authoritative |
                       |   Proxy   |  From: com_server   |     Server    |
                       +-----------+    To: Auth_server  +---------------+
                             ^                                   | all responses
            From: Rec_server |                                   | (sport: 53)
              To: com_server |                                   V
                       +-----------+                     +---------------+
                       | Recursive |                     | Authoritative |
                       |    TUN    |                     |      TUN      |
                       +-----------+                     +---------------+
                             ^                                   | From: Auth_server 
                 all queries |                                   |   To: com_server
                  (dport:53) |                                   V
    +---------+        +-----------+  From: com_server   +---------------+
    |  Stub   |------> | Recursive |    To: Rec_server   | Authoritative |
    |(clients)|<------ |  Server   |<--------------------|     Proxy     |
    +---------+        +-----------+                     +---------------+

# OPTIONS

`--auth_addr` *IP*
:   IP address of the authoritative server, IPv4 only

`--rec_addr` *IP*
:   IP address of the recursive server, IPv4 only

`--tun_name` *NAME*
:   name of the TUN interface; default is *dnstun*

`--num_threads` *NUMBER*
:   number of threads, default is 1

`--proxy_type` *TYPE*
:   type of the proxy, **recursive** (default) or **authoritative**

`--dry_run`
:   output debug message without running the proxy, default is false

# EXAMPLES

For example, with
network tunnel name *dnstun*,
recursive server address *192.168.1.10*,
authoritative server address *192.168.1.11*:

at recursive server:

    sudo ./dns-replay-proxy --proxy_type=recursive --tun_name=dnstun --auth_addr=192.168.1.11 --rec_addr=192.168.1.10 --num_threads=3

at authoritative server:

    sudo ./dns-replay-proxy --proxy_type=authoritative --tun_name=dnstun --auth_addr=192.168.1.11 --rec_addr=192.168.1.10 --num_threads=3

# INSTALLATION

To build, type *make*.

It requires the following packages on Fedora:
   glog-devel
   gflags-devel

# ALSO SEE

dns-replay-controller(1), dns-replay-client(1), dns-query-mutator(1)

# CHANGES
* 1.0, 2018-10-05: Beta release
