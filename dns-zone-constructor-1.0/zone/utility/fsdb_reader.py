import sys, os
from str_utility import print_err

class FsdbReader(object):
    def __init__(self, file_name):
        self.idx2name = {}
        self.fn = file_name
        self.f = None
        if not os.path.isfile(file_name):
            sys.exit('[error] file [%s] does not exist' % file_name)
        self.f = open(file_name)
        self._process_header()

    def _process_header(self):
        i = 0
        filter_list = ['#fsdb', '-F', 't']
        l = self.next_line()
        w = l.split()
        for p in w:
            if not p in filter_list:
                self.idx2name[i] = p
                i += 1

    def __del__(self):
        self.close()

    def close(self):
        if not self.f is None:
            self.f.close()

    def reset(self):
        self.close()
        self.f = open(self.fn)

    def next_line(self):
        l = self.f.readline()
        return l.strip()

    def next_line_list(self):
        l = self.next_line()
        while l.startswith('#'):
            l = self.next_line()
        w = l.split()
        return w

    def next_line_map(self, m):
        w = self.next_line_list()
        for idx, val in enumerate(w):
            m[self.idx2name[idx]] = val

    def print_metadata(self):
        print_err('# ' + str(self.idx2name))
