#!/bin/sh
netcosm -a test test &
./tests/gen_data.sh | telnet localhost 1234
