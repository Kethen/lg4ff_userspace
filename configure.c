#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <hidapi/hidapi.h>

#include "logging.h"
#include "device_ids.h"

// ref https://github.com/berarma/new-lg4ff/blob/master/hid-lg4ff.c

static void set_range_g25(hid_device *hd, int range){
	if(range < 40){
		STDERR("desired range %d is too small, setting 40 instead\n", range);
		range = 40;
	}
	if(range > 900){
		STDERR("desired range %d is too big, setting 900 instead\n", range);
		range = 900;
	}
	uint8_t cmd[6];
	cmd[0] = 0xf8;
	cmd[1] = 0x81;
	cmd[2] = range & 0x00ff;
	cmd[3] = (range & 0xff00) >> 8;
	cmd[4] = 0x00;
	cmd[5] = 0x00;
	cmd[6] = 0x00;
	int ret = hid_write(hd, cmd, sizeof(cmd));
	if(ret == -1){
		char error_buf[128];
		wcstombs(error_buf, hid_error(hd), sizeof(error_buf));
		STDERR("failed writing range setting command, %s\n", error_buf);
		exit(1);
	}
	STDOUT("sent range setting command for range %d\n", range);
}

int set_range(hid_device *hd, uint16_t product_id, int range){
	switch(product_id){
		case USB_DEVICE_ID_LOGITECH_G29_WHEEL:
			set_range_g25(hd, range);
			return 0;
		case USB_DEVICE_ID_LOGITECH_G27_WHEEL:
			set_range_g25(hd, range);
			return 0;
		case USB_DEVICE_ID_LOGITECH_G25_WHEEL:
			set_range_g25(hd, range);
			return 0;
		default:
			return 1;
	}
}
