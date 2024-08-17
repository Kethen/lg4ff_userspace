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
	uint8_t cmd[7] = {0};
	cmd[0] = 0xf8;
	cmd[1] = 0x81;
	cmd[2] = range & 0x00ff;
	cmd[3] = (range & 0xff00) >> 8;
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
			STDERR("range adjustment was not implemented for %s(0x%04d)\n", get_name_by_product_id(product_id), product_id);
			return 1;
	}
}

static void set_autocenter_default(hid_device *hd, int gain){
	// range check was done on main

	// first disable
	uint8_t cmd[7] = {0};
	cmd[0] = 0xf5;

	int ret = hid_write(hd, cmd, sizeof(cmd));
	if(ret == -1){
		char error_buf[128];
		wcstombs(error_buf, hid_error(hd), sizeof(error_buf));
		STDERR("failed sending auto center disabling command, %s\n", error_buf);
		exit(1);
	}

	if(gain == 0){
		return;
	}

	// set strength
	// also what is happening here :O
	uint32_t expand_a;
	uint32_t expand_b;
	if(gain <= 0xaaaa){
		expand_a = 0x0c * gain;
		expand_b = 0x80 * gain;
	}else{
		expand_a = (0x0c * 0xaaaa) + 0x06 * (gain - 0xaaaa);
		expand_b = (0x80 * 0xaaaa) + 0xff * (gain - 0xaaaa);
	}
	expand_a = expand_a >> 1;

	memset(cmd, 0x00, 7);
	cmd[0] = 0xfe;
	cmd[1] = 0x0d;
	cmd[2] = expand_a / 0xaaaa;
	cmd[3] = expand_a / 0xaaaa;
	cmd[4] = expand_b / 0xaaaa;

	ret = hid_write(hd, cmd, sizeof(cmd));
	if(ret == -1){
		char error_buf[128];
		wcstombs(error_buf, hid_error(hd), sizeof(error_buf));
		STDERR("failed sending auto center gain command, %s\n", error_buf);
		exit(1);
	}

	// enable auto center
	memset(cmd, 0x00, 7);
	cmd[0] = 0x14;
	ret = hid_write(hd, cmd, sizeof(cmd));
	if(ret == -1){
		char error_buf[128];
		wcstombs(error_buf, hid_error(hd), sizeof(error_buf));
		STDERR("failed sending auto center enable command, %s\n", error_buf);
		exit(1);
	}
}

int set_auto_center(hid_device *hd, uint16_t product_id, int gain){
	switch(product_id){
		case USB_DEVICE_ID_LOGITECH_G29_WHEEL:
			set_autocenter_default(hd, gain);
			return 0;
		case USB_DEVICE_ID_LOGITECH_G27_WHEEL:
			set_autocenter_default(hd, gain);
			return 0;
		case USB_DEVICE_ID_LOGITECH_G25_WHEEL:
			set_autocenter_default(hd, gain);
			return 0;
		default:
			STDERR("auto center adjustment was not implemented for %s(0x%04d)\n", get_name_by_product_id(product_id), product_id);
			return 1;
	}
}
