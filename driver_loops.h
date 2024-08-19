#ifndef __DRIVER_LOOPS_H
#define __DRIVER_LOOPS_H

#include <stdbool.h>

#include "probe.h"

struct loop_context{
	struct hidraw_device device;
	int gain;
	int auto_center;
	int spring_level;
	int damper_level;
	int friction_level;
	int range;
	bool hide_effects;
};

void start_loops(struct loop_context context);

#endif
