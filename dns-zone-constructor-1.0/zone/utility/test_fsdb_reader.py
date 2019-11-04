import unittest
from fsdb_reader import FsdbReader

header = 'msgid time srcip srcport dstip dstport protocol id'
lines = ['1 0.1 1.1.1.1 12345 2.2.2.2 53 udp 123456', '2 0.2 2.2.2.2 53 1.1.1.1 12345 udp 123456']
srcip = ['1.1.1.1','2.2.2.2']
dstip = ['2.2.2.2','1.1.1.1']
time = ['0.1', '0.2']

class TestFsdbReader(unittest.TestCase):
    def test_header(self):
        db = FsdbReader('./message.fsdb')
        m = db.idx2name
        self.assertNotEqual(len(m), 0)
        h = []
        for i in range(0, len(m)):
            h.append(m[i])
        self.assertEqual(' '.join(h), header)

    def test_line(self):
        db = FsdbReader('./message.fsdb')
        m = db.idx2name
        x = 0;
        while True:
            l = {}
            db.next_line_map(l)
            if len(l) == 0:
                break
            s = []
            for i in range(0, len(m)):
                s.append(l[m[i]])
            self.assertEqual(' '.join(s), lines[x])
            x += 1

    def test_field(self):
        db = FsdbReader('./message.fsdb')
        i = 0
        while True:
            l = {}
            db.next_line_map(l)
            if len(l) == 0:
                break
            self.assertEqual(l['srcip'], srcip[i])
            self.assertEqual(l['dstip'], dstip[i])
            self.assertEqual(l['time'], time[i])
            i += 1                                                     
            
if __name__ == '__main__':
    unittest.main()
