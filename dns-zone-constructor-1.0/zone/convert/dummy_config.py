import sys, os
from str_utility import write_file
from str_utility import print_err

def simple_config(a):
    return '''options {
    directory "/var/cache/bind";
    dnssec-validation no;
    listen-on port 53 {127.0.0.1; %s;};
    listen-on-v6 {none;};
    recursion no;
    notify no;
    allow-transfer { none; };
};''' % a

def dummy_config(a, p):
    return '''%s
view com {
    match-clients { 1.1.1.1/24; };
    zone "com" { type master; file "%sdb.com";};
};
view example {
    match-clients { 2.2.2.2/24; };
    zone "example.com" { type master; file "%sdb.example.com";};
};
''' % (simple_config(a), p, p)

def dummy(auth_addr, config_file, zone_dir, data_path):
    write_file(config_file, dummy_config(auth_addr, data_path))
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
