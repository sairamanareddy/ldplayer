## What is this?

This is a set of scripts that set up port-based routing and
`dns-replay-proxy` for replaying queries against a recursive server in
LDplayer.

## What are the files?

exp_setup_route_proxy.sh: a script to setup routing and run
`dns-replay-proxy` at remote servers

settings.sh: settings for exp_setup_route_proxy.sh

route_setup.sh: a script to setup routing at local machine

generate_config.py: a script to generate zone files and BIND
configuration for testing purpose only

## How to run it?

change the settings in `settings.sh` (the descriptions of the
variables are in `settings.sh`):

    # You must fill in the following required information 
    px_src=""
    rec_name=""
    rec_addr=""
    rec_interface=""
    rec_port=""
    auth_name=""
    auth_addr=""
    auth_interface=""
    auth_port=""
    
    # You may change the following information if necessary
    table=12
    mark=12
    tun="dnstun"
    routing_script="./route_setup.sh"

make sure you can ssh into the recursive and authoritative servers
and run the script:

    ./exp_setup_route_proxy.sh -p -r

If you just want to run proxies without setting up routing:

    ./exp_setup_route_proxy.sh -p

If you just want to set up routing without running proxies:

    ./exp_setup_route_proxy.sh -r

Alternatively, you can copy `route_setup.sh` and `dns-replay-proxy`
source code to the recursive and authoritative servers, and run them
respectively.
