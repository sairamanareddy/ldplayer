# Copyright (C) 2017-2018 by the University of Southern California
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.  

CC=g++
CFLAGS=-O3 -std=c++11 -Wall #-DDEBUG -g
#CFLAGS=-O3 -std=c++11 -Wall -DDEBUG -g -D_GLIBCXX_USE_CXX11_ABI=0
LFLAGS= -lldns -ltrace -lprotobuf #-levent -lpthread
SOURCES=$(wildcard *.cc)
OBJECTS=$(patsubst %.cc,%.o,$(SOURCES))
CSOURCES=$(wildcard *.c)
COBJECTS=$(patsubst %.c,%.o,$(CSOURCES))
EXECUTABLE=dns-query-mutator
LD_LIB_PATH=
ALL_OBJECTS = dns_msg.pb.o $(subst dns_msg.pb.o,,$(OBJECTS) $(COBJECTS))

all: $(EXECUTABLE)

.PHONY:
	clean all test

$(EXECUTABLE): $(ALL_OBJECTS)
	$(CC) -o $@ $(ALL_OBJECTS) $(LFLAGS) $(LD_LIB_PATH)

#%.o: %.c
.cc.o:
	$(CC) $(CFLAGS) -c $< -o $@
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf *~ $(ALL_OBJECTS) $(EXECUTABLE)

$(EXECUTABLE).1: README.md
	pandoc -f markdown -t man -o $@ $< -s

readme: $(EXECUTABLE).1
	man ./$<

# generate dns_msg.pb.{h,cc}
dns_msg:
	protoc -I=./ --cpp_out=./ ./dns_msg.proto

dns_msg.pb.o: dns_msg
	$(CC) $(CFLAGS) -c dns_msg.pb.cc -o $@

