#!/usr/bin/env bash

set -e

############################################################
# general information
############################################################
dn="xyz.isi.deterlab.net"
work_dir="/users/xyz/LDplayer_example"
exp_id="ldplayer_test"
exp_remote_dir="/tmp/${exp_id}"
result_dir="$exp_remote_dir/results"

############################################################
# authoritative server information
############################################################
# server's hostname
aut_box="s.$dn"

# server's IP address
aut_addr="10.1.1.2"

# server port that accepts DNS queries
aut_port="53"

# shortcut for ssh
aut="ssh $aut_box"

# short hostname
aut_hostname=`$aut "hostname -s"`

############################################################
# query client information
############################################################
# a list of client machines
client_box=()
for i in `seq 0 2`; do
    client_box+=("n-$i.$dn")
done

clt_exe="dns-replay-client"
clt_src="$work_dir/software/$clt_exe"
clt_tcp_timeout="0"

num_client=${#client_box[@]}
echo "$num_client clients"

############################################################
# controller information
############################################################
cntlr_exe="dns-replay-controller"
cntlr_src="$work_dir/software/$cntlr_exe"
cntlr_box="c.$dn"
cntlr_addr="10.1.1.3"
cntlr_port="10053"
cntlr_input_type="raw"
cntlr_input="xzcat $work_dir/input_trace/abc1.ldplayer.xz $work_dir/input_trace/abc2.ldplayer.xz $work_dir/input_trace/abc3.ldplayer.xz"
cntlr="ssh $cntlr_box"
cntlr_hostname=`$cntlr "hostname -s"`

############################################################
# run dns-replay-controller
############################################################
echo "preparing $cntlr_exe"

$cntlr "mkdir -p $exp_remote_dir"
$cntlr "rm -rf $exp_remote_dir/*"
$cntlr "mkdir -p $result_dir"
cntlr_remote_dir="$exp_remote_dir/$cntlr_exe"
cntlr_remote_exe="$cntlr_remote_dir/$cntlr_exe"

scp -r $cntlr_src $cntlr_box:$cntlr_remote_dir &> /dev/null 

$cntlr "cd $cntlr_remote_dir && make clean && make" &> /dev/null 
$cntlr "ls $cntlr_remote_exe" > /dev/null

echo "done preparing $cntlr_exe"

cntlr_output_prefix="$result_dir/cntlr_${cntlr_hostname}"
cntlr_output="${cntlr_output_prefix}.output"
cntlr_stdout="${cntlr_output_prefix}.stdout"
cntlr_stderr="${cntlr_output_prefix}.stderr"

$cntlr "killall $cntlr_exe" || true

$cntlr "nohup $cntlr_input | $cntlr_remote_exe --trace_limit 15 --input ${cntlr_input_type}:- --address $cntlr_addr --port $cntlr_port --num_clients $num_client >/dev/null 2>$cntlr_stderr &"

echo "$cntlr_exe $cntlr_box is running"

############################################################
# run dns-replay-client
############################################################
idx=${!client_box[*]}
for i in $idx; do
    nm=${client_box[$i]}
    sm=`echo $nm | cut -f 1 -d .`
    
    echo "preparing dns-replay-client at $nm"
    clt="ssh $nm"

    $clt "mkdir -p $exp_remote_dir"
    $clt "rm -rf $exp_remote_dir/*"
    $clt "mkdir -p $result_dir"
    
    clt_remote_dir="$exp_remote_dir/$clt_exe"
    clt_remote_exe="$clt_remote_dir/$clt_exe"

    scp -r $clt_src $nm:$exp_remote_dir &> /dev/null 

    $clt "cd $clt_remote_dir && make clean && make" &> /dev/null 
    $clt "ls $clt_remote_exe" > /dev/null

    echo "done preparing dns-replay-client at $nm"

    prefix="$result_dir/clt_$sm"
    clt_output="${prefix}.output"
    clt_stdout="${prefix}.stdout"
    clt_stderr="${prefix}.stderr"

    $clt "killall $clt_exe" || true
    
    $clt "nohup $clt_remote_exe -c adaptive -g disable -d -t $clt_tcp_timeout -s $aut_addr:$aut_port -r $cntlr_addr:$cntlr_port -v >/dev/null 2>$clt_stderr -o timing:$clt_output &"
    echo "client $nm is running"
done

exit 0
