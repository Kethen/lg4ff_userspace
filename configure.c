#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <hidapi/hidapi.h>

#include "logging.h"
#include "device_ids.h"

// ref https://github.com/berarma/new-lg4ff/blob/master/hid-lg4ff.c

static void set_range_dfp(hid_device *hd, int range){
	int start_left, start_right, full_range;

	if(range < 40){
		STDERR("desired range %d is too small, setting 40 instead\n", range);
		range = 40;
	}
	if(range > 900){
		STDERR("desired range %d is too big, setting 900 instead\n", range);
		range = 900;
	}
	uint8_t cmd[7];

	/* Prepare "coarse" limit command */
	cmd[0] = 0xf8;
	cmd[1] = 0x00;	/* Set later */
	cmd[2] = 0x00;
	cmd[3] = 0x00;
	cmd[4] = 0x00;
	cmd[5] = 0x00;
	cmd[6] = 0x00;

	if (range > 200) {
		cmd[1] = 0x03;
		full_range = 900;
	} else {
		cmd[1] = 0x02;
		full_range = 200;
	}
	int ret = hid_write(hd, cmd, 7);
	if(ret == -1){
		char error_buf[128];
		wcstombs(error_buf, hid_error(hd), sizeof(error_buf));
		STDERR("failed writing coarse range setting command, %s\n", error_buf);
		exit(1);
	}
	STDOUT("sent coarse range setting command for range %d\n", range);

	/* Prepare "fine" limit command */
	cmd[0] = 0x81;
	cmd[1] = 0x0b;
	cmd[2] = 0x00;
	cmd[3] = 0x00;
	cmd[4] = 0x00;
	cmd[5] = 0x00;
	cmd[6] = 0x00;

	if (range != 200 && range != 900) {
		/* Construct fine limit command */
		start_left = (((full_range - range + 1) * 2047) / full_range);
		start_right = 0xfff - start_left;

		cmd[2] = start_left >> 4;
		cmd[3] = start_right >> 4;
		cmd[4] = 0xff;
		cmd[5] = (start_right & 0xe) << 4 | (start_left & 0xe);
		cmd[6] = 0xff;
	}

	ret = hid_write(hd, cmd, 7);
	if(ret == -1){
		char error_buf[128];
		wcstombs(error_buf, hid_error(hd), sizeof(error_buf));
		STDERR("failed writing fine range setting command, %s\n", error_buf);
		exit(1);
	}
	STDOUT("sent fine range setting command for range %d\n", range);
}

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
		case USB_DEVICE_ID_LOGITECH_DFP_WHEEL:
			set_range_dfp(hd, range);
			return 0;
		default:
			STDERR("range adjustment was not implemented for %s(0x%04x)\n", get_name_by_product_id(product_id), product_id);
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
	STDOUT("sent auto center disable command\n");

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
	STDOUT("sent auto center gain command for gain %d\n", gain);

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

	STDOUT("sent auto center enable command\n");
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
		case USB_DEVICE_ID_LOGITECH_DFP_WHEEL:
			set_autocenter_default(hd, gain);
			return 0;
		default:
			STDERR("auto center adjustment was not implemented for %s(0x%04x)\n", get_name_by_product_id(product_id), product_id);
			return 1;
	}
}
