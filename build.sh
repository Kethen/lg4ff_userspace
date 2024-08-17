set -xe

CC=${CC:-gcc}
CPPC=${CC:-g++}

$CC -g -c probe.c -o probe.o
$CC -g -c device_ids.c -o device_ids.o
$CC -g -c driver_loops.c -o driver_loops.o
$CC -g -c switch_mode.c -o switch_mode.o
$CC -g -c main.c -o main.o
$CC probe.o device_ids.o driver_loops.o switch_mode.o main.o -o lg4ff_userspace -lhidapi-hidraw

rm *.o
