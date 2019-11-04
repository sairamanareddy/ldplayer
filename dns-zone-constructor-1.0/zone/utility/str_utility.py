import sys

def print_err_clean(s):
    sys.stderr.write(s)
    
def print_err(s):
    sys.stderr.write(s + '\n')

def warn(s):
    print_err('[warn] ' + s)

def err(s):
    print_err('[error] ' + s)

def write_file(fn, s):
    f = open(fn, 'w')
    f.write(s)
    f.close()

def add_last_dot(s):
    if len(s) == 0 or s.endswith('.'):
        return s
    return s + '.' 

def get_zone_name(s):
    if len(s) == 0 or s == '.':
        return s
    s = add_last_dot(s)
    zone_name = '.'.join(s.split('.')[1:])
    return zone_name if len(zone_name) != 0 else '.'
