set -xe

CC=${CC:-gcc}
CPPC=${CC:-g++}

$CC -g -c probe.c -o probe.o
$CC -g -c device_ids.c -o device_ids.o
$CC -g -c driver_loops.c -o driver_loops.o
$CC -g -c switch_mode.c -o switch_mode.o
$CC -g -c configure.c -o configure.o
$CC -g -c force_feedback.c -o force_feedback.o
$CC -g -c main.c -o main.o
$CC probe.o device_ids.o driver_loops.o switch_mode.o configure.o force_feedback.o main.o -o lg4ff_userspace -lhidapi-hidraw -lm
$CC probe.o device_ids.o driver_loops.o switch_mode.o configure.o force_feedback.o main.o -o lg4ff_userspace_usb -lhidapi-libusb -lm

rm *.o
