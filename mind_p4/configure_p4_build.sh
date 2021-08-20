#!/bin/bash
$SDE/pkgsrc/p4-build/configure --with-tofino --with-p4c=p4c --prefix=$SDE_INSTALL \
	--bindir=$SDE_INSTALL/bin \
	P4_NAME=$1 \
        P4_PATH=$SDE/pkgsrc/p4-examples/p4_16_programs/$1/$1.p4 \
        P4_VERSION=p4-16 P4_ARCHITECTURE=tna P4FLAGS=--parser-timing-reports\
        LDFLAGS="-L$SDE_INSTALL/lib"
