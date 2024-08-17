#ifndef __DRIVER_LOOPS_H
#define __DRIVER_LOOPS_H

#include "probe.h"

struct loop_context{
	struct hidraw_device device;
};

void start_loops(struct loop_context context);

#endif
