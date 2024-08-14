#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include <hidapi/hidapi.h>

#include "logging.h"
#include "device_ids.h"

#include "probe.h"


int find_wheels(struct hidraw_device *hidraw_devices, int buffer_size){
	struct hid_device_info *devs = hid_enumerate(0, 0);
	struct hid_device_info *cur = devs;
	int wheels_found = 0;
	while(cur != NULL){
		if(wheels_found == buffer_size){
			break;
		}
		if((uint16_t)cur->vendor_id != (uint16_t)USB_VENDOR_ID_LOGITECH){
			cur = cur->next;
			continue;
		}
		for(int i = 0;i < sizeof(supported_ids) / sizeof(int); i++){
			if((int16_t)cur->product_id == (int16_t)supported_ids[i]){
				hidraw_devices[wheels_found].vendor_id = (uint16_t)cur->vendor_id;
				hidraw_devices[wheels_found].product_id = (uint16_t)cur->product_id;
				wcstombs(hidraw_devices[wheels_found].name_buf, cur->product_string, 256);
				hidraw_devices[wheels_found].name_buf[255] = '\0';
				wheels_found++;
				break;
			}
		}
		cur = cur->next;
	}
	hid_free_enumeration(devs);
	return wheels_found;
}
