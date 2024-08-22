#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <hidapi/hidapi.h>

#include "logging.h"
#include "device_ids.h"
#include "probe.h"

// ref https://github.com/berarma/new-lg4ff/blob/master/hid-lg4ff.c

void switch_mode(struct hidraw_device device, uint32_t target_product_id){
	hid_device *hd = hid_open_path(device.backend_path);
	if(hd == NULL){
		char error_buf[128];
		wcstombs(error_buf, hid_error(NULL), sizeof(error_buf));
		STDERR("failed opening hid device at %s, %s\n", device.backend_path, error_buf);
		exit(1);
	}

	int ret;

	switch(target_product_id){
		case USB_DEVICE_ID_LOGITECH_G29_WHEEL:{
			uint8_t cmd[] = {0xf8, 0x09, 0x05, 0x01, 0x01, 0x00, 0x00};
			ret = hid_write(hd, cmd, sizeof(cmd));
			break;
		}
		case USB_DEVICE_ID_LOGITECH_G27_WHEEL:{
			uint8_t cmd[] = {0xf8, 0x09, 0x04, 0x01, 0x00, 0x00, 0x00};
			ret = hid_write(hd, cmd, sizeof(cmd));
			break;
		}
		case USB_DEVICE_ID_LOGITECH_G25_WHEEL:{
			uint8_t cmd[] = {0xf8, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00};
			ret = hid_write(hd, cmd, sizeof(cmd));
			break;
		}
		case USB_DEVICE_ID_LOGITECH_DFP_WHEEL:{
			uint8_t cmd[] = {0xf8, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
			ret = hid_write(hd, cmd, sizeof(cmd));
			break;
		}
		default:{
			STDERR("unknown product id 0x%04x for mode switching\n", target_product_id);
			exit(1);
		}
	}
	if(ret == -1){
		char error_buf[128];
		wcstombs(error_buf, hid_error(hd), sizeof(error_buf));
		STDERR("failed sending mode set command, %s\n", error_buf);
		exit(1);
	}
	STDOUT("mode set command sent\n");
}
