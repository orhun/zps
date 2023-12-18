#!/usr/bin/env bash

set -e

project_dir="$(pwd)/.."
# Compile the example & run
cd "${project_dir}/example"
gcc -O3 -Wall zproc.c -o z.o
./z.o &>/dev/null &
./z.o &>/dev/null &
# Compile the main source & run
cd "${project_dir}/src"
gcc -fprofile-arcs -ftest-coverage -s -O3 -Wall -Wextra -pedantic zps.c -o zps
./zps -v && ./zps -h && printf '1' | ./zps -p
./zps -x && ./zps -lr
./zps -r && ./zps -s
# Print code coverage information
gcov zps.c
# Send report to codecov
[ "$UPLOAD" == 'true' ] && bash <(curl -s https://codecov.io/bash)
# Cleanup
rm -v zps zps.c.* zps.gc*
