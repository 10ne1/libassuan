#!/bin/bash

./autogen.sh
LDFLAGS=-static ./configure --enable-maintainer-mode
make
