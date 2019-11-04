#!/usr/bin/env python
import sys, os, argparse

def warn(s):
    sys.stderr.write('[warn] ' + s + '\n')

def err(s):
    sys.stderr.write('[error] ' + s + '\n')

def simple_config(a):
    return '''options {
    directory "/var/cache/bind";
    dnssec-validation no;
    listen-on port 53 {127.0.0.1; %s;};
    recursion no;
    allow-transfer { none; };
};''' % a

def dummy_config(a):
    return '''%s
view com {
    match-clients { 1.1.1.1/24; };
    zone "com" { type master; file "/etc/bind/zones/db.com";};
};
view example {
    match-clients { 2.2.2.2/24; };
    zone "example.com" { type master; file "/etc/bind/zones/db.example.com";};
};
''' % (simple_config(a))

def write_file(fn, s):
    f = open(fn, 'w')
    f.write(s)
    f.close()

def dummy_file(auth_addr, config_file, zone_dir, verbose):
    if verbose > 1:
        warn('this is to generate dummy testing files')
    write_file(config_file, dummy_config(auth_addr))
    com_zone = '''@\t900\tIN\tSOA\ta.gtld-servers.net. nstld.verisign-grs.com. 1445651506 1800 900 604800 86400
example.com.\t172800\tIN\tNS\tns.example.com.
ns.example.com.\t172800\tIN\tA\t2.2.2.8
com.\t172800\tIN\tNS\ta.gtld-servers.net.
'''
    write_file(zone_dir+'/db.com', com_zone)
    example_zone = '''$TTL\t604800
@\t60\tIN\tSOA\tns.example.com. admin.example.com. 106196653 900 900 1800 60
example.com.\tIN\tNS\tns.example.com.
ns\tIN\tA\t2.2.2.8
@\tIN\tA\t2.2.2.9
www\tIN\tA\t2.2.2.1
'''
    write_file(zone_dir+'/db.example.com', example_zone)

def worker(auth_addr, in_file, config_file, zone_dir, is_test, verbose):
    print "ok"

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='generate zone files and Bind configuration files')
    parser.add_argument('-a', '--address', type=str, help='authoritative IP address', required=True)
    parser.add_argument('-i', '--infile', type=str, help='input file')
    parser.add_argument('-c', '--config', type=str, help='output for Bind configuration file', required=True)
    parser.add_argument('-z', '--zone', type=str, help='output directory for zone files', required=True)
    parser.add_argument('-t', '--test', action='store_true', help='generate dummy files for testing')
    parser.add_argument('-v', '--verbose', action='count', help='output more details', default=0)
    parser.add_argument('-V', '--version', action='version', version='%(prog)s 1.0')
    args = parser.parse_args()

    if not os.path.isdir(args.zone):
        sys.exit('[error] [%s] is not a directory, abort!' % args.zone)

    if args.test:
        dummy_file(args.address, args.config, args.zone, args.verbose)
    else:
        worker(args.address, args.infile, args.config, args.zone, args.test, args.verbose)
