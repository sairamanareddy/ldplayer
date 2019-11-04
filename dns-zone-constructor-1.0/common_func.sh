#!/usr/bin/env bash

echoerr() { echo -e "$@" 1>&2; }

err() { echoerr "[error] $1"; }

errx() { echoerr "[error] $1"; exit 1; }

info() { echoerr "[info] $1"; }

dbg() { echoerr "[debug] $1"; }

cmd_mkdir() {
    for var in "$@"; do
	mkdir -p -v $var
    done
}

string_not_empty() {
    if [ -z "$2" ]
    then
	errx "$1 is empty"
    fi
}

file_exist() {
    if [ ! -f "$1" ]
    then
	errx "file [$1] does not exist"
    fi
}

directory_exist() {
    if [ ! -d "$1" ]
    then
	errx "directory [$1] does not exist"
    fi
}
