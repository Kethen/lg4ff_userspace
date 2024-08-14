set -xe

CC=${CC:-gcc}
CPPC=${CC:-g++}

$CC -g -c probe.c -o probe.o
$CC -g -c main.c -o main.o
$CC main.o probe.o -o lg4ff_userspace -lhidapi-hidraw

rm *.o
