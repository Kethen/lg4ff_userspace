set -xe

CC=${CC:-gcc}
CPPC=${CC:-g++}

if [ "$DEBUG" == "true" ]
then
	$CC -g -c probe.c -o probe.o
	$CC -g -c device_ids.c -o device_ids.o
	$CC -g -c driver_loops.c -o driver_loops.o
	$CC -g -c switch_mode.c -o switch_mode.o
	$CC -g -c configure.c -o configure.o
	$CC -g -c force_feedback.c -o force_feedback.o
	$CC -g -c log_effect.c -o log_effect.o
	$CC -g -c main.c -o main.o
	$CC probe.o device_ids.o driver_loops.o switch_mode.o configure.o force_feedback.o log_effect.o main.o -o lg4ff_userspace -lhidapi-hidraw -lm
	$CC probe.o device_ids.o driver_loops.o switch_mode.o configure.o force_feedback.o log_effect.o main.o -o lg4ff_userspace_usb -lhidapi-libusb -lm
else
	$CC probe.c device_ids.c driver_loops.c switch_mode.c configure.c force_feedback.c log_effect.c main.c -O2 -o lg4ff_userspace -lhidapi-hidraw -lm
	$CC probe.c device_ids.c driver_loops.c switch_mode.c configure.c force_feedback.c log_effect.c main.c -O2 -o lg4ff_userspace_usb -lhidapi-libusb -lm
fi

rm -f *.o
