## What is this?

This is a set of scripts that generate zone files in order to replay
queries against a recursive server in LDplayer.

*dns-zone-constructor* can generate zone files and DNS server
configurations using network traces captured at a *recursive* server.

As of 2018-10-07, *dns-zone-constructor* does not support generating
zone files using network traces captured at an *authoritative* server.

Replaying queries to an *authoritative* server requires obtaining the
zone files of the *authoritative* server from its operator.

## What are the files?

generate_zone_files.sh: the main script to generate zone files

settings.sh: settings for generate_zone_files.sh

zone: a set of python scripts that generate zone files from the output
of dnsanon

named.root: root hint file

## How to run it?

Prerequisite: you must have the following software installed:

* [dnsanon](https://ant.isi.edu/software/dnsanon/index.html
  "dnsanon"): We have pre-built versions of dnsanon for RPM (Fedora,
  CentOS, REHL); see
  [HERE](https://copr.fedorainfracloud.org/coprs/johnh/dnsanon/)

* [BIND](https://www.isc.org/downloads/bind/ "BIND"): In Fedora, BIND
  can be installed via `sudo dnf install bind`.

* dig and tcpdump: they are installed by default in most of the Linux
  distributions.

After installing the required software:

1. Configure BIND in recursive mode and make sure name server
   control works:

        sudo rndc flush
        sudo rndc reload

2. You might want to get an up-to-date version of the root hint file
   from [here](https://www.internic.net/domain/named.root).  The one
   included in this package is probably the same as up-to-date version
   since root hint data rarely changes.

3. Create a file that lists the queries as the input data for zone
   construction. Each line in the file contains two data elements
   (query name and query type) delimited by a single space, like
   \"google.com A\".

4. You need to change the settings (`settings.sh`) accordingly based on
   your operating system. More details are included in `settings.sh`

5. Make sudo password timeout longer, as long as possible only during
   the zone construction, since you need to run tcpdump for each
   query. Some instructions about changing sudo timeout are
   [here](https://www.tecmint.com/set-sudo-password-timeout-session-longer-linux/).

6. After changing settings, run ./generate_zone_files.sh. You may
   check log file in `./log`.

The output directory is set in `settings.sh`. By default, the
generated zone files are at `sample_data/zone_output/zones` and the
configure file for split-horizon authoritative server is at
`./sample_data/zone_output/named.conf`.

When you run split-horizon authoritative server later in LDplayer, you
might need to manually change `directory "/var/cache/bind"` in this
generated BIND configure file based on your own usage.
