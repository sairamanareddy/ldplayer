import sys, os
import dns.message

class DNSMessage(object):
    def __init__(self):
        self.src_ip = ''
        self.src_port = ''
        self.dst_ip = ''
        self.dst_port = ''
        self.dns_id = ''
        self.rcode = ''
        self.opcode = ''
        
        self.data = None

        # a list of strings
        self.header = []
        self.question = []
        self.answer = []
        self.authority = []
        self.additional = []

    def to_dnsmsg(self):
        self.data = dns.message.from_text(self.text())
        self.data.set_rcode(int(self.rcode))
        self.data.set_opcode(int(self.opcode))
        
    def str(self):
        s =  '# from: ' + self.src_ip + ':' + self.src_port + '\n'
        s += '# to:   ' + self.dst_ip + ':' + self.dst_port + '\n'
        s += self.text();
        return s
        
    def text(self):
        t = ''
        if len(self.header) > 0: # header
            t += ';HEADER\n' + '\n'.join(self.header) + '\n'
        if len(self.question) > 0: # question section
            t += ';QUESTION\n' + '\n'.join(self.question) + '\n'
        if len(self.answer) > 0: # answer section
            t += ';ANSWER\n' + '\n'.join(self.answer) + '\n'
        if len(self.authority) > 0: # authority section
            t += ';AUTHORITY\n' + '\n'.join(self.authority) + '\n'
        if len(self.additional) > 0: # additional section
            t += ';ADDITIONAL\n' + '\n'.join(self.additional) + '\n'
        return t
