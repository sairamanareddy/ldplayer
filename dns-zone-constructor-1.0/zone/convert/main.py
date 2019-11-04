import sys, os, argparse
from str_utility import print_err
from str_utility import warn
from converter import Converter
from dummy_config import dummy

if __name__ == '__main__':
    parser = argparse.ArgumentParser(prog='convert', description='generate zone files and configure files')
    parser.add_argument('-i', '--input-directory', type=str, help='input directory', dest='input_dir', required=True)
    parser.add_argument('-o', '--output-directory', type=str, help='output directory', dest='output_dir', required=True)
    parser.add_argument('-a', '--authoritative-address', type=str, help='authoritative IP address', dest='auth_addr', required=True)
    parser.add_argument('-s', '--recursive-address', type=str, help='recursive IP address in captured trace', dest='rec_addr', required=True)
    parser.add_argument('-c', '--config', type=str, help='output for Bind configuration file', dest='config_file', required=True)
    parser.add_argument('-r', '--root-hint', type=str, help='root hint file', dest='root_hint', required=True)
    parser.add_argument('-z', '--zone-path', type=str,
                        help='path to zone files in Bind configuration file, default is /etc/named/zone',
                        default='/etc/named/zone', dest='zone_path', required=False)    
    parser.add_argument('-t', '--test', action='store_true', help='generate dummy files for testing')
    parser.add_argument('-v', '--verbose', action='count', help='verbose log', default=0)
    parser.add_argument('-V', '--version', action='version', version='%(prog)s 1.0')
    args = parser.parse_args()

    if args.verbose > 1:
        print_err('# input directory: ' + args.input_dir)
        print_err('# output directory: ' + args.output_dir)
        print_err('# authoritative IP address: ' + args.auth_addr)
        print_err('# recursive IP address in trace: ' + args.rec_addr)
        print_err('# output Bind configuration file: ' + args.config_file)
        print_err('# root hint file: ' + args.root_hint)

    if not os.path.isdir(args.output_dir): # check if output directory exists
        sys.exit('[error] output directory (%s) does not exist' % args.output_dir)
    if not os.path.isfile(args.root_hint): # check if root hint file exists
        sys.exit('[error] root hint file (%s) does not exist' % args.root_hint)

    # add / to the end of path if not present
    if not args.zone_path.endswith('/'):
        args.zone_path += '/'
    if not args.output_dir.endswith('/'):
        args.output_dir += '/'

    if args.test:
        if args.verbose > 0: # only output samples
            warn('ignore input files; only create dummy sample output files')
        dummy(args.auth_addr, args.config_file, args.output_dir, args.zone_path)
    else:
        if not os.path.isdir(args.input_dir): # check input file
            sys.exit('[error] input directory (%s) does not exist' % args.input_dir)
        cvt = Converter(args.input_dir, args.output_dir, args.config_file, args.auth_addr, args.zone_path, args.root_hint, args.rec_addr);
        cvt.write_to_file()

