# Copyright (C) 2015-2018 by the University of Southern California
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
CFLAGS=-std=c++11 -Wall #-g -DDEBUG
LFLAGS=-lpthread -lglog -lgflags
SOURCES=$(wildcard *.cc)
OBJECTS=$(patsubst %.cc,%.o,$(SOURCES))
CSOURCES=$(wildcard *.c)
COBJECTS=$(patsubst %.c,%.o,$(CSOURCES))
EXECUTABLE=dns-replay-proxy

all: $(EXECUTABLE)

.PHONY:
	clean all

$(EXECUTABLE): $(OBJECTS) $(COBJECTS) #safe_queue.hh
	$(CC) -o $@ $(OBJECTS) $(COBJECTS) $(LFLAGS)

#%.o: %.c
.cc.o:
	$(CC) $(CFLAGS) -c $< -o $@
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf *~ $(OBJECTS) $(COBJECTS) $(EXECUTABLE)

$(EXECUTABLE).1: README.md
	pandoc -f markdown -t man -o $@ $< -s

readme: $(EXECUTABLE).1
	man ./$<

