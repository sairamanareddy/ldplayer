import sys, os
import dns.name
import dns.rdata
import dns.rdataset
import dns.rdataclass
import dns.rdatatype
import dns.zone

class DNSZone(object):
    def __init__(self, zone_name):
        self.address = set()
        origin = zone_name
        if zone_name == '.':
            zone_name = 'root.'
        # create a fake SOA record
        soa = origin + ' 60 IN SOA ns.' + zone_name + ' dns-admin.' + zone_name + ' 208775588 1800 900 604800 86400'
        self.data = dns.zone.from_text(soa, origin=origin, relativize=False, check_origin=False)

    def add_record(self, l, has_class=False):
        if len(l) == 0 or l.startswith(';') or l.startswith('#') or l == '\n':
            return
        # for example: A.ROOT-SERVERS.NET.      3600000      A     198.41.0.4
        w = l.split()
        owner_name = w[0].lower()
        ttl = int(w[1])
        rt = w[2].upper()
        rd = ' '.join(w[3:])
        if has_class:
            # for example: google.com.             300     IN      A       216.58.194.174
            rc = w[2].upper()
            rt = w[3].upper()
            rd = ' '.join(w[4:])
            if self.data.rdclass != dns.rdataclass.from_text(rc):
                sys.exit('[error] zone class [%s] != record class [%s]' % (dns.rdataclass.to_text(self.data.rdclass), rc))

        if 'TYPE65534' in rd.upper():
            return
        if rt == 'SOA' and owner_name != self.data.origin.to_text():
            return
        
        rdataset = self.data.find_rdataset(owner_name, dns.rdatatype.from_text(rt), create=True)
        rdata = dns.rdata.from_text(rdataset.rdclass, rdataset.rdtype, rd)
        rdataset.add(rdata, ttl)

    def get_record_rdata(self, name, rt):
        l = set()
        name = name.lower()
        rt = rt.upper()
        records = self.data.get_rdataset(name, rt)
        if records != None:
            for rd in records:
                l.add(rd.to_text())
        return l

    def get_rrset_str(self, name, rt):
        l = set()
        name = name.lower()
        rt = rt.upper()
        rrset = self.data.get_rrset(name, rt)
        if rrset != None:
            l = set(rrset.to_text(relativize=False).splitlines())
        return l
