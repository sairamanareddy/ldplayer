#!/usr/bin/env bash

# Copyright (C) 2018 by the University of Southern California
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.

set -e
echoerr() { echo -e "$@" 1>&2; }
usage() {
    echoerr "example: $0 -t TABLE -m MARK -n TUNNEL -i eth4 -f recursive"
}

# defule value
TABLE=12
MARK=12
TUN=dnstun
INT=eth4
FUNC=""

while getopts ":t:m:n:i:f:h" opt; do
    case $opt in
	t)
	    TABLE=$OPTARG
	    ;;
	m)
	    MARK=$OPTARG
	    ;;
	n)
	    TUN=$OPTARG
	    ;;
	i)
	    INT=$OPTARG
	    ;;
	f)
	    FUNC=$OPTARG
	    ;;
	h)
	    echoerr "info: $0 [-tmnifh]"
	    ;;
	\?)
	    echoerr "Invalid option: -$OPTARG"
	    exit 1
	    ;;
	:)
	    echoerr "Option -$OPTARG requires an argument."
	    exit 1
	    ;;
    esac
done

if [ -z $FUNC ] ; then
    echoerr "[error] -f must be provide"
    usage
    exit 1
fi 

if [ "$FUNC" != "recursive" ] && [ "$FUNC" != "authoritative" ] ; then
    echoerr "[error] -f only accepts \"recursive\" or \"authoritative\""
    exit 1
fi

# get the src ip address
MYIP=`ifconfig $INT | grep "inet addr:" | cut -d : -f 2 | cut -d ' ' -f 1`
if [ -z $MYIP ] ; then
    echoerr "[error] default interface does not work, try -i INT"
    exit 1
fi

echo "myip : $MYIP"
echo "tun  : $TUN"

# bring up tunnel
#sudo ifconfig $TUN down
echoerr "[info] tun is up"
sudo ip tuntap add dev $TUN mode tun
sudo ifconfig $TUN 172.16.0.1/16 up

# make the packets
# from r, dport 53 to tun
# from a, sport 53 to tun
WP="sport" # which port
if [ "$FUNC" == "recursive" ] ; then
    WP="dport"
fi
echoerr "[info] set port-based routing"
sudo iptables -A OUTPUT -t mangle -p udp -s $MYIP --${WP} 53 -j MARK --set-mark $MARK
sudo iptables -A OUTPUT -t mangle -p tcp -s $MYIP --${WP} 53 -j MARK --set-mark $MARK

# set up routing rule
sudo ip route add default via 172.16.0.1 dev $TUN table $TABLE
sudo ip rule add from $MYIP fwmark $MARK table $TABLE

# show info
echoerr "ip rule:"
ip rule show | grep $TABLE

echoerr "\ntable:"
ip route show table $TABLE

echoerr "\niptables:"
sudo iptables -L -t mangle

exit 0
