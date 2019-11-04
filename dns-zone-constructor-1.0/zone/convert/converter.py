import sys, os
import dns.rdatatype
import dns.flags
from dns_message import DNSMessage
from dns_zone import DNSZone
from fsdb_reader import FsdbReader
from str_utility import print_err
from str_utility import warn
from str_utility import write_file
from str_utility import get_zone_name
from dummy_config import simple_config

class Converter(object):
    def __init__(self, input_dir, output_dir, config_file, auth_addr, zone_path, root_hint_file, rec_addr):
        if not os.path.isdir(input_dir):
            sys.exit('[error] directory [%s] does not exist' % input_dir)
        self.input_dir = input_dir
        self.output_dir = output_dir
        self.config_file = config_file
        self.auth_addr = auth_addr
        self.zone_path = zone_path
        self.rec_addr = rec_addr
        self.SPT = '\t'
        self.total_processed_dir = 0

        # map zone name to DNSZone, for example, {'.':DNSZone, 'com':DNSZone}
        self.name2zone = {}
        
        # map IP to a set of zone name for example, {IP:{'com','net'}}
        self.addr2name = {}

        # create root zone first because recursive will bootstrap with the knowledge of root address.
        # the first query out of the recursive without cache is sent to one of the root servers
        self.process_root_hint(root_hint_file)
        
        # This assumes that each input sub-directory contains the network trace of
        # full iterative queries for a single incoming query at a recursive server.
        self.sub_dir = [d for d in os.listdir(input_dir) if os.path.isdir(os.path.join(input_dir,d))]
        for d in self.sub_dir:
            self.process_dir(d)
            
    def add_addr2name(self, addr, name):
        if not addr in self.addr2name:
            self.addr2name[addr] = set()
        self.addr2name[addr].add(name.lower())

    def add_name2zone(self, name):
        name = name.lower()
        if not name in self.name2zone:
            self.name2zone[name] = DNSZone(name)
            
    def process_root_hint(self, fn):        
        if not os.path.isfile(fn):
            sys.exit('[error] root hint file [%s] does not exist' % fn)
        if '.' in self.name2zone:
            sys.exit('[error] root zone exist!')
        self.add_name2zone('.')   # create a new root zone

        # as of 2018-09-13, Both '.' and 'root-servers.net.' are hosted at the root servers.
        # zone name "root-servers.net." need to change if the names of
        # the root server ({a-j}.root-servers.net.) change.
        root_servers_ns = 'root-servers.net.'
        self.add_name2zone(root_servers_ns)   # create a new root-servers.net. zone
        
        with open(fn) as rtf:     # add all the record to the root zone
            for l in rtf:
                l = l.strip()
                if len(l) == 0 or l.startswith(';') or l.startswith('#') or l == '\n':
                    continue        
                self.name2zone['.'].add_record(l)
                w = l.split()
                if w[2].upper() == 'NS':
                    w[0] = root_servers_ns
                    self.name2zone[root_servers_ns].add_record(' '.join(w))
                
        ns_records = self.name2zone['.'].get_record_rdata('.', 'NS') # find all the NS names
        if len(ns_records) == 0:
            return
        for ns in ns_records:
            a_records = self.name2zone['.'].get_record_rdata(ns, 'A') # find all the A records
            if len(a_records) > 0:
                for a in a_records:
                    self.name2zone['.'].address.add(a)
                    self.add_addr2name(a, '.')
                    self.name2zone[root_servers_ns].address.add(a)
                    self.add_addr2name(a, root_servers_ns)
            aaaa_records = self.name2zone['.'].get_record_rdata(ns, 'AAAA') # find all the AAAA records
            if len(aaaa_records) > 0: 
                for aaaa in aaaa_records:
                    self.name2zone['.'].address.add(aaaa)
                    self.add_addr2name(aaaa, '.')
                    self.name2zone[root_servers_ns].address.add(aaaa)
                    self.add_addr2name(aaaa, root_servers_ns)

    # some names have different DNS hierarchies in reality:
    # for example, as of 2018-09-22, ebc.net.tw. has two different hierarchies:
    # (A) root-> tw -> NS of ebc.net.tw -> answer <-+
    #                                               |
    # (B) root-> tw -> net.tw -> NS of ebc.net.tw --+
    # and tw and net.tw are hosted at the same servers (such as h.dns.tw and i.dns.tw).
    # The traces used for zone construction may only have (A).
    # Some other names with the same SLD net.tw might introduce net.tw NS in tw zone.
    # As a result, the third-level domain delegation of (A) will be broken
    # since we don't have (see) NS of ebc.net.tw in net.tw zone
    def fix_delegation(self, zs):
        for z in zs:
            if z == '.':
                continue
            records = self.name2zone[z].data.to_text(relativize=False).split('\n')
            for record in records:
                record = record.strip()
                w = record.split()
                if len(record) == 0 or record == '\n' or w[3].upper() != 'NS':
                    continue
                tmp_z = z
                for t in zs:
                    if t != z and t != '.' and t != w[0] and w[0].endswith(t) and len(t) > len(tmp_z):
                        tmp_z = t
                if tmp_z != z:
                    ns_records = self.name2zone[z].get_rrset_str(tmp_z, 'NS')
                    if len(ns_records) > 0:
                        self.name2zone[tmp_z].add_record(record, has_class=True)
                        warn('# fix_delegation: add record [%s] from zone [%s] to zone [%s]' % (record, z, tmp_z))
                        ns_server = w[-1]
                        if ns_server.endswith(tmp_z):
                            a_records = self.name2zone[z].get_rrset_str(ns_server, 'A')
                            if len(a_records) > 0:
                                for a in a_records:
                                    self.name2zone[tmp_z].add_record(a, has_class=True)
                                    warn('# fix_delegation: add record [%s] from zone [%s] to zone [%s]' % (a, z, tmp_z))
                            aaaa_records = self.name2zone[z].get_rrset_str(ns_server, 'AAAA')
                            if len(aaaa_records) > 0:
                                for aaaa in aaaa_records:
                                    self.name2zone[tmp_z].add_record(aaaa, has_class=True)
                                    warn('# fix_delegation: add record [%s] from zone [%s] to zone [%s]' % (aaaa, z, tmp_z))
                        
    def write_to_file(self):
        # combine the servers (identified by addresses) that has same set of zones
        views = {}
        for a in self.addr2name:
            s = self.addr2name[a]
            k = '|'.join(sorted(s))
            if not k in views:
                views[k] = set()
            views[k].add(a)

        view_count = 0
        with open(self.config_file, 'w') as f:
            f.write(simple_config(self.auth_addr) + '\n')
            for k in views:
                view_count += 1
                f.write('view v' + str(view_count) + ' {\n')
                f.write('    match-clients { ')
                for a in views[k]:
                    f.write('%s; ' % a)
                f.write('};\n')
                zs = k.split('|')
                for z in zs:
                    f.write('    zone \"%s\" { type master; file \"%s\"; };\n' %
                            (z, self.zone_path + ('root_zone' if z == '.' else z)))
                f.write('};\n')
                self.fix_delegation(zs)

        # write zone files
        for n in self.name2zone:
            try:
                self.name2zone[n].data.check_origin()
            except dns.exception.DNSException as e:
                print_err('# [exception] %s: %s' % (n, str(e)))
            write_file(self.output_dir + '/' + ('root_zone' if n == '.' else n), self.name2zone[n].data.to_text(relativize=False))
        print_err('# total zone files: %d' % len(self.name2zone))
        print_err('# total processed directory: %d' % self.total_processed_dir)
        
    def process_dir(self, d):
        self.total_processed_dir += 1
        print_err('# %d: %s' % (self.total_processed_dir, d))
        rrs = DNSZone('.') # use DNSZone for each lookup API; it does not mean to be a zone
        dns_msg = {}  # map msgid to DNSMessage
        self.read_dns_message(d, dns_msg, rrs)
        # print rrs.data.to_text(relativize=False)
        self.convert_dns_message(dns_msg)
        self.process_dns_message(dns_msg, rrs)

    def process_dns_message(self, dns_msg, rrs):
        for k in dns_msg: # create a zone for a NS name and map the address
            self.create_zone(dns_msg[k].data.answer, rrs)
            self.create_zone(dns_msg[k].data.authority, rrs)
            self.create_zone(dns_msg[k].data.answer, rrs)

        ks = dns_msg.keys()
        ks.sort(key=int)
        for k in ks:      # push the data to each zone
            #print dns_msg[k].str()
            self.push_zone(dns_msg[k].data.answer, dns_msg[k].src_ip, rrs)
            self.push_zone(dns_msg[k].data.authority, dns_msg[k].src_ip, rrs)
            self.push_zone(dns_msg[k].data.additional, dns_msg[k].src_ip, rrs)
                            
    def create_zone(self, rrsets, rrs):
        for rrset in rrsets:
            if rrset.rdtype == dns.rdatatype.from_text('NS'):
                for rd in rrset:
                    ns = rd.to_text().lower()
                    zone_name = str(rrset.name).lower()
                    self.add_name2zone(zone_name)
                    a_records = rrs.get_record_rdata(ns, 'A') # find all the A records
                    if len(a_records) > 0:
                        for a in a_records:
                            self.name2zone[zone_name].address.add(a)
                            self.add_addr2name(a, zone_name)
                    aaaa_records = rrs.get_record_rdata(ns, 'AAAA') # find all the AAAA records
                    if len(aaaa_records) > 0: 
                        for aaaa in aaaa_records:
                            self.name2zone[zone_name].address.add(aaaa)
                            self.add_addr2name(aaaa, zone_name)
                    if len(a_records) == 0 and len(aaaa_records) == 0:
                        # This might be OK in the cases where additional section is truncated and addresses are missing.
                        # The recursive will retry or resolve those NS names by itself.
                        warn('create_zone: cannot find address for NS [%s], zone name [%s]' % (ns, zone_name))

                    # add NS record for zone name if possible
                    ns_records = rrs.get_rrset_str(zone_name, 'NS')
                    if len(ns_records) > 0:
                        for record in ns_records:
                            self.name2zone[zone_name].add_record(record, has_class=True)

                    # add address record for the owner of the NS record which is in the same zone
                    if ns == zone_name or ns.endswith((zone_name if zone_name == '.' else ('.' + zone_name))):
                        a_records = rrs.get_rrset_str(ns, 'A')
                        if len(a_records) > 0:
                            for a in a_records:
                                self.name2zone[zone_name].add_record(a, has_class=True)
                        aaaa_records = rrs.get_rrset_str(ns, 'AAAA')
                        if len(aaaa_records) > 0: 
                            for aaaa in aaaa_records:
                                self.name2zone[zone_name].add_record(aaaa, has_class=True)
                        if len(a_records) == 0 and len(aaaa_records) == 0:
                            warn('create_zone: cannot find address for NS [%s] in same zone of [%s]' % (ns, zone_name))
                        
    def push_zone(self, rrsets, src_ip, rrs):
        for rrset in rrsets:
            records = rrset.to_text(relativize=False).splitlines()
            for record in records:
                w = record.split()
                z = self.find_zone(src_ip, w[0])
                if z != None:
                    z.add_record(record, has_class=True)
                else:
                    print self.addr2name[src_ip]
                    warn('push_zone: cannot find zone for [%s], [%s]' % (src_ip, record))

    def find_zone(self, src_ip, name):
        name = name.lower()
        z = None
        if not src_ip in self.addr2name:
            sys.exit('[error] %s is not in record' % src_ip)
        # find the longest zone name that matches name
        zone_name_len = -1
        for n in self.addr2name[src_ip]:
            if name == n:
                z = self.name2zone[n]
                break;
            if name.endswith((n if n == '.' else ('.' + n))) and len(n) > zone_name_len:
                zone_name_len = len(n)
                if not n in self.name2zone:
                    sys.exit('[error] %s is not in name2zone' % n)
                z = self.name2zone[n]
        return z
    
    def convert_dns_message(self, dns_msg):
        for msgid, msg in dns_msg.iteritems():
            msg.to_dnsmsg()
            #print '-------------' + msgid + '-------------'
            #print msg.data.to_text()
            # print msg.str()
            
    def read_dns_message(self, d, dns_msg, rrs):
        # read message fsdb
        msg_db = FsdbReader(self.fmt_path(d, '.message.fsdb'))
        while True:
            l = {}
            msg_db.next_line_map(l)
            if len(l) == 0:
                break
            if 'msgid' not in l or 'id' not in l:
                sys.exit('[error] msgid or id not found')

            # discard any queries, and responses from recursive server
            if l['qr'] == '0' or l['srcip'] == self.rec_addr:
                continue
            
            if not l['msgid'] in dns_msg:
                dns_msg[l['msgid']] = DNSMessage()
                # dns_msg[l['msgid']] = DNSMessage(dns_id=int(l['id']))
                # dns_msg[l['msgid']].set_opcode(int(l['opcode']))
                # dns_msg[l['msgid']].set_rcode(int(l['rcode']))
            dns_msg[l['msgid']].src_ip = l['srcip']
            dns_msg[l['msgid']].dst_ip = l['dstip']
            dns_msg[l['msgid']].src_port = l['srcport']
            dns_msg[l['msgid']].dst_port = l['dstport']
            dns_msg[l['msgid']].dns_id = l['id']
            dns_msg[l['msgid']].rcode = l['rcode']
            dns_msg[l['msgid']].opcode = l['opcode']
            dns_msg[l['msgid']].header.append("id " + l['id'])
            flag_list = ''
            if l['qr'] == '1':
                flag_list += 'QR '
            if l['aa'] == '1':
                flag_list += 'AA '
            if l['tc'] == '1':
                flag_list += 'TC '
            if l['rd'] == '1':
                flag_list += 'RD '
            if l['ra'] == '1':
                flag_list += 'RA '
            if l['ad'] == '1':
                flag_list += 'AD '
            if l['cd'] == '1':
                flag_list += 'CD '
            if len(flag_list) > 0:
                dns_msg[l['msgid']].header.append("flags " + flag_list)
        msg_db.close()

        # read question fsdb
        q_db = FsdbReader(self.fmt_path(d, '.question.fsdb'))
        while True:
            l = {}
            q_db.next_line_map(l)
            if len(l) == 0:
                break
            if 'msgid' not in l:
                sys.exit('[error] msgid is not found in question fsdb')
            if not l['msgid'] in dns_msg:
                #warn(l['msgid'] + ' is not in dns_msg, not use')
                continue
            dns_msg[l['msgid']].question.append(l['name'] + self.SPT + l['class'] + self.SPT + l['type'])
        q_db.close()

        # read rr fsdb
        rr_db = FsdbReader(self.fmt_path(d, '.rr.fsdb'))
        while True:
            l = {}
            rr_db.next_line_map(l)
            if len(l) == 0:
                break
            if 'msgid' not in l:
                sys.exit('[error] msgid is not found in question fsdb')
            if not l['msgid'] in dns_msg:
                #warn(l['msgid'] + ' is not in dns_msg, not use')
                continue
            if 'TYPE65534' in l['rdata'].upper(): # dnspython cannot handle this
                continue
            if 'TYPE0' == l['type'] or 'CLASS0' == l['class']: # abnormal
                continue
            #.                       518400  IN      NS      i.root-servers.net.
            record = l['name'] + self.SPT + l['ttl'] + self.SPT + l['class'] + self.SPT + l['type'] + self.SPT + l['rdata'].replace('%20', ' ')
            rrs.add_record(record, has_class=True)
            if l['section_name'] == 'ANS':
                dns_msg[l['msgid']].answer.append(record)
            elif l['section_name'] == 'AUT':
                dns_msg[l['msgid']].authority.append(record)
            elif l['section_name'] == 'ADD':
                dns_msg[l['msgid']].additional.append(record)
            else:
                sys.exit('unknow section name:%s' % l['section_name'])
        rr_db.close()            

    def fmt_path(self, d, s):
        return self.input_dir + '/' + d + '/' + d + s
