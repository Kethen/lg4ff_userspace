#include <stdio.h>
#include <stdlib.h>

#include "logging.h"
#include "probe.h"

static void list_devices(struct hidraw_device *hidraw_devices, int entries){
	for(int i = 0;i < entries; i++){
		STDOUT("Device %d:\n", i + 1);
		STDOUT(" name: %s\n", hidraw_devices[i].name_buf);
		STDOUT(" path: %s\n", hidraw_devices[i].path_buf);
		STDOUT(" vendor id: 0x%04x\n", hidraw_devices[i].vendor_id);
		STDOUT(" product id: 0x%04x\n", hidraw_devices[i].product_id);
	}
}

int main(int argc, char** argv){
	struct hidraw_device hidraw_devices[256];
	int wheels_found = find_wheels(hidraw_devices, sizeof(hidraw_devices) / sizeof(struct hidraw_device));
	list_devices(hidraw_devices, wheels_found);
    return 0;
}
