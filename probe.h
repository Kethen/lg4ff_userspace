#ifndef __PROBE_H
#define __PROBE_H

#include <stdint.h>

struct hidraw_device{
	uint16_t vendor_id;
	uint16_t product_id;
	char name_buf[256];
};

int find_wheels(struct hidraw_device *hidraw_devices, int buffer_size);

#endif
