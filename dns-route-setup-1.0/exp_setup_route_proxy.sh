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

# this script is to set up the environment for recursive trace replay
# route setup (-r) should be only done once

echoerr() { echo -e "$@" 1>&2; }
err() { echoerr "[error] $1"; }
err_exit() { echoerr "[error] $1"; exit 1; }
info() { echoerr "[info] $1"; }
dbg() { echoerr "[debug] $1"; }

usage() {
    echoerr "usage: $0"
    echoerr " -p    setup proxy"
    echoerr " -r    setup port-based routing"
    echoerr " -t    setup test zone"
    echoerr " -d    dry run"
    echoerr "example: $0 -r -p -t"
    exit 1
}

if [ -s settings.sh ]; then
    . ./settings.sh
fi

set -e

: ${rec_name:?}
: ${rec_addr:?}
: ${rec_interface:?}

: ${auth_name:?}
: ${auth_addr:?}
: ${auth_interface:?}

: ${table:?}
: ${mark:?}
: ${tun:?}

: ${px_src:?}
: ${px_exe:?}

: ${routing_script:?}

ssh_options="-o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no"
rec="ssh $ssh_options $rec_name"
auth="ssh $ssh_options $auth_name"

setup_proxy="false"
setup_routing="false"
setup_test_zone="false"
dry_run="false"

while getopts ":prtdh" opt; do
    case $opt in
	p)
	    setup_proxy="true"
	    ;;
	r)
	    setup_routing="true"
	    ;;
	t)
	    setup_test_zone="true"
	    ;;
	d)
	    dry_run="true"
	    ;;
        h)
	    usage
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

dbg "setup_proxy=${setup_proxy}"
dbg "setup_routing=${setup_routing}"
dbg "setup_test_zone=${setup_test_zone}"
dbg "dry_run=${dry_run}"

[ ! -d $px_src ] && err_exit "proxy code directory [$px_src] is not valid"
info "recursive:\t$rec_name\t$rec_addr\t$rec_interface"
info "authoritative:\t$auth_name\t$auth_addr\t$auth_interface"

[ ! -x $routing_script ] && err_exit "routing script [$routing_script] is not valid"

bn=$(basename $0)
id=${bn%.*}
remote_dir="/tmp/$id-$(date +%s)"
remote_routing_script="$remote_dir/$(basename $routing_script)"
remote_proxy_dir=$remote_dir/$(basename $px_src)
remote_proxy="$remote_proxy_dir/$px_exe"
remote_proxy_log="$remote_dir/proxy.log"

dbg "remote_dir=${remote_dir}"
dbg "remote_routing_script=${remote_routing_script}"
dbg "remote_proxy_dir=${remote_proxy_dir}"
dbg "remote_proxy=${remote_proxy}"
dbg "remote_proxy_log=${remote_proxy_log}"

$rec "rm -rf $remote_routing_script /tmp/$id-*"
$auth "rm -rf $remote_routing_script /tmp/$id-*"

$rec "mkdir -pv $remote_dir" &> /dev/null
$auth "mkdir -pv $remote_dir" &> /dev/null

if [ "$setup_routing" == "false" ]; then
    info "skip setup routing..."
else
    scp $routing_script $rec_name:$remote_routing_script &> /dev/null
    scp $routing_script $auth_name:$remote_routing_script &> /dev/null
    if [ "$dry_run" == "false" ]; then
	$rec "sudo $remote_routing_script -t $table -m $mark -n $tun -i $rec_interface -f recursive"
	$auth "sudo $remote_routing_script -t $table -m $mark -n $tun -i $auth_interface -f authoritative"
    fi
    info "Done setup port-based routing"
fi

if [ "$setup_proxy" == "false" ]; then
    info "skip setup proxy..."
else
    scp -r $px_src $rec_name:$remote_dir &> /dev/null
    scp -r $px_src $auth_name:$remote_dir &> /dev/null
    $rec "cd $remote_proxy_dir && make clean && make && ls $remote_proxy" &> /dev/null
    $auth "cd $remote_proxy_dir && make clean && make && ls $remote_proxy" &> /dev/null
    $rec "sudo killall $px_exe" || true
    $auth "sudo killall $px_exe" || true
    if [ "$dry_run" == "false" ]; then
	arg_str="--tun_name=$tun --auth_addr=$auth_addr --rec_addr=$rec_addr --num_threads=3 --v=3"
	$rec "sudo nohup $remote_proxy --proxy_type=recursive ${arg_str} &> $remote_proxy_log &"
	$auth "sudo nohup $remote_proxy --proxy_type=authoritative ${arg_str} &> $remote_proxy_log &"
    fi
    info "remote proxies are running [$remote_proxy] with log [$remote_proxy_log]"
fi

if [ "$setup_test_zone" == "false" ]; then
    info "skip setup test zone..."
else
    mkdir -vp dummy/zones
    python generate_config.py -a $auth_addr -z dummy/zones -c dummy/named.conf -t -vv
    scp -r dummy $auth_name:$remote_dir &> /dev/null
    if [ "$dry_run" == "false" ]; then
	$auth "sudo mkdir -pv /etc/bind/zones"
	$auth "sudo cp $remote_dir/dummy/zones/db.com /etc/bind/zones/db.com"
	$auth "sudo cp $remote_dir/dummy/zones/db.example.com /etc/bind/zones/db.example.com"
	$auth "sudo cp $remote_dir/dummy/named.conf /etc/bind/named.conf"
	#info "restart Bind at authoritative"
	$auth "sudo systemctl restart bind9"
    fi
    info "generated dummy zone file and BIND configure file for testing"
fi

exit 0
