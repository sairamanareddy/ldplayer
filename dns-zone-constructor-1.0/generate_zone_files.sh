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

############################################################
# This script is to generate zone files by three steps.
#
# 1. get network trace
# 
# 2. parse network trace
#
# 3. generate zone files
#
# NOTE:
# - You might want to change some of the Linux command below based on your operating system
#   (this script was tested under Fedora 28)
# - You might want to make sudo password timeout longer
#   https://www.tecmint.com/set-sudo-password-timeout-session-longer-linux/
# - You might want to add OPTIONS="-4" in /etc/sysconfig/named to disable IPv6
#   https://www.sbarjatiya.com/notes_wiki/index.php/Disabling_IPv6_lookups_in_bind
############################################################

set -e

if [ -s common_func.sh ]; then
    . ./common_func.sh
fi

if [ -s settings.sh ]; then
    . ./settings.sh
else
    echo "[error] $0: settings.sh does not exist, abort!"
    exit 1
fi

# log
: ${id:?}
: ${log_dir:?}
log_file="$log_dir/$0.$id.log"
tcpdump_log="$log_dir/tcpdump.$id.log"

# for query
: ${input_query:?}
: ${trace_output:?}
: ${tcpdump_user:?}
: ${interface_name:?}
: ${server_process:?}
: ${server_address:?}

# for parse network trace
: ${dnsanon:?}
: ${dnsanon_output:?}

# for zone
: ${zone_output:?}
: ${zone_path:?}
: ${config_output:?}
: ${root_hint:?}
: ${exp_server_address:?}

# check settings and clean up
file_exist $input_query
file_exist $dnsanon
cmd_mkdir $trace_output $dnsanon_output $zone_output $log_dir
rm -rf $trace_output/* $dnsanon_output/* $zone_output/* $log_file $tcpdump_log

# log settings
echo "# id=$id" >> $log_file
echo "# log_dir=$log_dir" >> $log_file
echo "# input_query=$input_query" >> $log_file
echo "# trace_output=$trace_output" >> $log_file
echo "# tcpdump_user=$tcpdump_user" >> $log_file
echo "# tcpdump_log=$tcpdump_log" >> $log_file
echo "# interface_name=$interface_name" >> $log_file
echo "# server_process=$server_process" >> $log_file
echo "# server_address=$server_address" >> $log_file
echo "# dnsanon=$dnsanon" >> $log_file
echo "# dnsanon_output=$dnsanon_output" >> $log_file
echo "# zone_output=$zone_output" >> $log_file
echo "# zone_path=$zone_path" >> $log_file
echo "# config_output=$config_output" >> $log_file
echo "# root_hint=$root_hint" >> $log_file
echo "# exp_server_address=$exp_server_address" >> $log_file

############################################################
# First step, get network trace
############################################################
echo "First step, get network trace" >> $log_file

sudo systemctl stop $server_process
sudo systemctl start $server_process
sudo pkill tcpdump || true

while IFS= read -r line
do
    echo "########## $line ##########" >> $log_file
    
    # split query name and query type
    query_name=$(echo $line | cut -d ' ' -f 1)
    query_type=$(echo $line | cut -d ' ' -f 2)
    
    # start tcpdump
    trace_file="$trace_output/${query_name}+${query_type}.pcap"
    echo "########## $line ##########" >> $tcpdump_log
    sudo nohup tcpdump --immediate-mode -nn -s 0 -B 81910 -i $interface_name -Z $tcpdump_user -w $trace_file "port 53" &>> $tcpdump_log &

    # reload server, clear cache
    if [ "$server_process" == "named" ] || [ "$server_process" == "bind9" ]
    then
	sudo rndc flush &>> $log_file
	sudo rndc reload &>> $log_file
    else
	sudo unbound-control reload &>> $log_file
    fi

    sleep 2s
    # dig
    dig -4 +tries=1 $line @"$server_address" +timeout=10 &>> $log_file
    
    # stop tcpdump
    sleep 2s
    sudo pkill tcpdump
    #pid=$(ps -e | pgrep tcpdump)
    #sudo kill -2 $pid
    
done <"$input_query"

sudo systemctl stop $server_process

############################################################
# Second step, parse network trace
############################################################
echo "Second step, parse network trace" >> $log_file

for f in $trace_output/*.pcap
do
    bn=$(basename $f)
    fn="${bn%.*}"
    mkdir -p -v $dnsanon_output/$fn
    $dnsanon -i $f -o $dnsanon_output/$fn -f $fn -c none
done

############################################################
# Third step, generate zone files
############################################################
echo "Third step, generate zone files" >> $log_file

./zone/bin/convert -i $dnsanon_output -o $zone_output -a $exp_server_address -c $config_output -r $root_hint -s $server_address -z $zone_path -vv &> $log_dir/zone_convert.$id.log

exit 0
