#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>

#include <hidapi/hidapi.h>

#include "probe.h"
#include "device_ids.h"
#include "driver_loops.h"
#include "logging.h"
#include "configure.h"
#include "force_feedback.h"

// ref https://github.com/berarma/new-lg4ff/blob/master/hid-lg4ff.c

#define SETUP_KEY(f, k){ \
	ioctl(f, UI_SET_EVBIT, EV_KEY); \
	ioctl(f, UI_SET_KEYBIT, k); \
}

#define SETUP_AXIS(f, a, min, max){ \
	ioctl(f, UI_SET_EVBIT, EV_ABS); \
	ioctl(f, UI_SET_ABSBIT, a); \
	struct uinput_abs_setup uas = {0}; \
	uas.code = a; \
	uas.absinfo.minimum = min; \
	uas.absinfo.maximum = max; \
	ioctl(f, UI_ABS_SETUP, &uas); \
}

#define SETUP_FFB(f, k){ \
	ioctl(f, UI_SET_EVBIT, EV_FF); \
	ioctl(f, UI_SET_EVBIT, EV_FF_STATUS); \
	ioctl(f, UI_SET_FFBIT, k); \
}

#define EMIT_INPUT(f, t, c, v){ \
	struct input_event e; \
	e.type = t; \
	e.code = c; \
	e.value = v; \
	write(f, &e, sizeof(e)); \
}

static bool get_bit(const uint8_t *buf, int bit_num, int buf_len){
	int byte_offset = bit_num / 8;
	int local_bit = bit_num % 8;
	if(byte_offset >= buf_len){
		STDERR("%s: bit_num %d is too big for buf_len %d\n", __func__, bit_num, buf_len);
		exit(1);
	}
	uint8_t mask = 1 << local_bit;
	return (buf[byte_offset] & mask) ? 1 : 0;
}

static void uinput_g29_setup(int uinput_fd, struct loop_context *context){
	// 25 buttons numerated from 1 to 25 on hid
	// wine just goes ascending so map all 25 onto trigger happy during evdev -> hid
	for(int i = 0;i < 25; i++){
		int key = BTN_TRIGGER_HAPPY + i;
		SETUP_KEY(uinput_fd, key);
	}

	// X axis on 0-65535, wine seems to convert them by name during evdev -> hid
	SETUP_AXIS(uinput_fd, ABS_X, 0, 65535);
	// Y on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_Y, 0, 255);
	// Z on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_Z, 0, 255);
	// Rx is a dummy
	SETUP_AXIS(uinput_fd, ABS_RX, 0, 255);
	// Ry is a dummy
	SETUP_AXIS(uinput_fd, ABS_RY, 0, 255);
	// Rz on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_RZ, 0, 255);


	// HAT/dpad
	SETUP_AXIS(uinput_fd, ABS_HAT0X, -1, 1);
	SETUP_AXIS(uinput_fd, ABS_HAT0Y, -1, 1);

	// ffb
	SETUP_FFB(uinput_fd, FF_CONSTANT);
	if(!context->hide_effects){
		SETUP_FFB(uinput_fd, FF_SPRING);
		SETUP_FFB(uinput_fd, FF_DAMPER);
		SETUP_FFB(uinput_fd, FF_AUTOCENTER);
		SETUP_FFB(uinput_fd, FF_PERIODIC);
		SETUP_FFB(uinput_fd, FF_SINE);
		SETUP_FFB(uinput_fd, FF_SQUARE);
		SETUP_FFB(uinput_fd, FF_TRIANGLE);
		SETUP_FFB(uinput_fd, FF_SAW_UP);
		SETUP_FFB(uinput_fd, FF_SAW_DOWN);
		SETUP_FFB(uinput_fd, FF_RAMP);
		SETUP_FFB(uinput_fd, FF_FRICTION);
	}

	struct uinput_setup usetup = {0};
	usetup.id.bustype = BUS_USB;
	usetup.id.vendor = USB_VENDOR_ID_LOGITECH;
	usetup.id.product = USB_DEVICE_ID_LOGITECH_G29_WHEEL;
	usetup.ff_effects_max = LG4FF_MAX_EFFECTS;
	strcpy(usetup.name, "userspace G29");

	ioctl(uinput_fd, UI_DEV_SETUP, &usetup);
	ioctl(uinput_fd, UI_DEV_CREATE);
}

static void uinput_g27_setup(int uinput_fd, struct loop_context *context){
	// 22 buttons numerated from 1 to 22 on hid
	for(int i = 0;i < 22; i++){
		int key = BTN_TRIGGER_HAPPY + i;
		SETUP_KEY(uinput_fd, key);
	}

	// X axis on 0-16383, wine seems to convert them by name during evdev -> hid
	SETUP_AXIS(uinput_fd, ABS_X, 0, 16383);
	// Y on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_Y, 0, 255);
	// Z on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_Z, 0, 255);
	// Rx is a dummy
	SETUP_AXIS(uinput_fd, ABS_RX, 0, 255);
	// Ry is a dummy
	SETUP_AXIS(uinput_fd, ABS_RY, 0, 255);
	// Rz on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_RZ, 0, 255);

	// HAT/dpad
	SETUP_AXIS(uinput_fd, ABS_HAT0X, -1, 1);
	SETUP_AXIS(uinput_fd, ABS_HAT0Y, -1, 1);

	// ffb
	SETUP_FFB(uinput_fd, FF_CONSTANT);
	if(!context->hide_effects){
		SETUP_FFB(uinput_fd, FF_SPRING);
		SETUP_FFB(uinput_fd, FF_DAMPER);
		SETUP_FFB(uinput_fd, FF_AUTOCENTER);
		SETUP_FFB(uinput_fd, FF_PERIODIC);
		SETUP_FFB(uinput_fd, FF_SINE);
		SETUP_FFB(uinput_fd, FF_SQUARE);
		SETUP_FFB(uinput_fd, FF_TRIANGLE);
		SETUP_FFB(uinput_fd, FF_SAW_UP);
		SETUP_FFB(uinput_fd, FF_SAW_DOWN);
		SETUP_FFB(uinput_fd, FF_RAMP);
		SETUP_FFB(uinput_fd, FF_FRICTION);
	}

	struct uinput_setup usetup = {0};
	usetup.id.bustype = BUS_USB;
	usetup.id.vendor = USB_VENDOR_ID_LOGITECH;
	usetup.id.product = USB_DEVICE_ID_LOGITECH_G27_WHEEL;
	usetup.ff_effects_max = LG4FF_MAX_EFFECTS;
	strcpy(usetup.name, "userspace G27");

	ioctl(uinput_fd, UI_DEV_SETUP, &usetup);
	ioctl(uinput_fd, UI_DEV_CREATE);
}

static void uinput_g25_setup(int uinput_fd, struct loop_context *context){
	// 19 buttons numerated from 1 to 19 on hid
	for(int i = 0;i < 19; i++){
		int key = BTN_TRIGGER_HAPPY + i;
		SETUP_KEY(uinput_fd, key);
	}

	// X axis on 0-16383, wine seems to convert them by name during evdev -> hid
	SETUP_AXIS(uinput_fd, ABS_X, 0, 16383);
	// Y on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_Y, 0, 255);
	// Z on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_Z, 0, 255);
	// Rx is a dummy
	SETUP_AXIS(uinput_fd, ABS_RX, 0, 255)
	// Ry is a dummy
	SETUP_AXIS(uinput_fd, ABS_RY, 0, 255)
	// Rz on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_RZ, 0, 255);

	// HAT/dpad
	SETUP_AXIS(uinput_fd, ABS_HAT0X, -1, 1);
	SETUP_AXIS(uinput_fd, ABS_HAT0Y, -1, 1);

	// ffb
	SETUP_FFB(uinput_fd, FF_CONSTANT);
	if(!context->hide_effects){
		SETUP_FFB(uinput_fd, FF_SPRING);
		SETUP_FFB(uinput_fd, FF_DAMPER);
		SETUP_FFB(uinput_fd, FF_AUTOCENTER);
		SETUP_FFB(uinput_fd, FF_PERIODIC);
		SETUP_FFB(uinput_fd, FF_SINE);
		SETUP_FFB(uinput_fd, FF_SQUARE);
		SETUP_FFB(uinput_fd, FF_TRIANGLE);
		SETUP_FFB(uinput_fd, FF_SAW_UP);
		SETUP_FFB(uinput_fd, FF_SAW_DOWN);
		SETUP_FFB(uinput_fd, FF_RAMP);
		SETUP_FFB(uinput_fd, FF_FRICTION);
	}

	struct uinput_setup usetup = {0};
	usetup.id.bustype = BUS_USB;
	usetup.id.vendor = USB_VENDOR_ID_LOGITECH;
	usetup.id.product = USB_DEVICE_ID_LOGITECH_G25_WHEEL;
	usetup.ff_effects_max = LG4FF_MAX_EFFECTS;
	strcpy(usetup.name, "userspace G25");

	ioctl(uinput_fd, UI_DEV_SETUP, &usetup);
	ioctl(uinput_fd, UI_DEV_CREATE);
}

static void uinput_dfp_setup(int uinput_fd, struct loop_context *context){
	// 14 buttons numerated from 1 to 14 on hid
	for(int i = 0;i < 14; i++){
		int key = BTN_TRIGGER_HAPPY + i;
		SETUP_KEY(uinput_fd, key);
	}

	// X axis on 0-16383, wine seems to convert them by name during evdev -> hid
	SETUP_AXIS(uinput_fd, ABS_X, 0, 16383);;
	// Y on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_Y, 0, 255);
	// Z is a dummy
	SETUP_AXIS(uinput_fd, ABS_Z, 0, 255);
	// Rx is a dummy
	SETUP_AXIS(uinput_fd, ABS_RX, 0, 255);
	// Ry is a dummy
	SETUP_AXIS(uinput_fd, ABS_RY, 0, 255);
	// Rz on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_RZ, 0, 255);

	// HAT/dpad
	SETUP_AXIS(uinput_fd, ABS_HAT0X, -1, 1);
	SETUP_AXIS(uinput_fd, ABS_HAT0Y, -1, 1);

	// ffb
	SETUP_FFB(uinput_fd, FF_CONSTANT);
	if(!context->hide_effects){
		SETUP_FFB(uinput_fd, FF_SPRING);
		SETUP_FFB(uinput_fd, FF_DAMPER);
		SETUP_FFB(uinput_fd, FF_AUTOCENTER);
		SETUP_FFB(uinput_fd, FF_PERIODIC);
		SETUP_FFB(uinput_fd, FF_SINE);
		SETUP_FFB(uinput_fd, FF_SQUARE);
		SETUP_FFB(uinput_fd, FF_TRIANGLE);
		SETUP_FFB(uinput_fd, FF_SAW_UP);
		SETUP_FFB(uinput_fd, FF_SAW_DOWN);
		SETUP_FFB(uinput_fd, FF_RAMP);
		SETUP_FFB(uinput_fd, FF_FRICTION);
	}

	struct uinput_setup usetup = {0};
	usetup.id.bustype = BUS_USB;
	usetup.id.vendor = USB_VENDOR_ID_LOGITECH;
	usetup.id.product = USB_DEVICE_ID_LOGITECH_DFP_WHEEL;
	usetup.ff_effects_max = LG4FF_MAX_EFFECTS;
	strcpy(usetup.name, "userspace Drive Force Pro");

	ioctl(uinput_fd, UI_DEV_SETUP, &usetup);
	ioctl(uinput_fd, UI_DEV_CREATE);
}

static void uinput_g29_emit(int uinput_fd, uint8_t *report_buf, uint8_t *last_report_buf, struct loop_context *context){
	// 12 byte report
	// first 4 bits are for the hat
	// second 25 bits are the buttons
	// 3 bits of padding
	// 16 bits of X
	// 8 bits of Z
	// 8 bits of Rz
	// 8 bits of Y
	// 8 bits of vendor
	// 8 bits of vendor
	// 8 bits of vendor
	uint8_t hat = report_buf[0] & 0x0f;
	uint8_t last_hat = last_report_buf[0] & 0x0f;
	if(hat != last_hat){
		switch(hat){
			case 0:
				// up only
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, -1);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, 0);
				break;
			case 1:
				// up and right
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, -1);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, 1);
				break;
			case 2:
				// right only
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, 0);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, 1);
				break;
			case 3:
				// down right
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, 1);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, 1);
				break;
			case 4:
				// down
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, 1);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, 0);
				break;
			case 5:
				// down left
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, 1);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, -1);
				break;
			case 6:
				// left
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, 0);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, -1);
				break;
			case 7:
				// up left
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, -1);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, -1);
				break;
			case 8:
				// center
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, 0);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, 0);
				break;
		}
	}
	for(int i = 0;i < 25; i++){
		int key = BTN_TRIGGER_HAPPY + i;
		int bit_num = i + 4;
		bool button_on = get_bit(report_buf, bit_num, 12);
		bool button_was_on = get_bit(last_report_buf, bit_num, 12);
		if(button_on != button_was_on){
			EMIT_INPUT(uinput_fd, EV_KEY, key, button_on? 1: 0);
		}
	}
	if(context->combine_pedals == 1){
		report_buf[6] = ((uint32_t)0xFF + report_buf[6] - report_buf[7]) >> 1;
		report_buf[7] = 0x7F;
	}
	if(context->combine_pedals == 2){
		report_buf[6] = ((uint32_t)0xFF + report_buf[6] - report_buf[8]) >> 1;
		report_buf[8] = 0x7F;
	}
	uint16_t x = *(uint16_t *)&report_buf[4];
	uint16_t last_x = *(uint16_t *)&last_report_buf[4];
	if(x != last_x){
		EMIT_INPUT(uinput_fd, EV_ABS, ABS_X, x);
	}
	if(report_buf[6] != last_report_buf[6]){
		EMIT_INPUT(uinput_fd, EV_ABS, ABS_Z, report_buf[6]);
	}
	if(report_buf[7] != last_report_buf[7]){
		EMIT_INPUT(uinput_fd, EV_ABS, ABS_RZ, report_buf[7]);
	}
	if(report_buf[8] != last_report_buf[8]){
		EMIT_INPUT(uinput_fd, EV_ABS, ABS_Y, report_buf[8]);
	}

	EMIT_INPUT(uinput_fd, EV_SYN, SYN_REPORT, 0);
	memcpy(last_report_buf, report_buf, 12);
}

static void uinput_g27_emit(int uinput_fd, uint8_t *report_buf, uint8_t *last_report_buf, struct loop_context *context){
	// 11 byte report
	// first 4 bits are for the hat
	// second 22 bits are the buttons (on g25, 19 bits of buttons, 3 bits of vendor)
	// 14 bits of X
	// 8 bits of Z
	// 8 bits of Rz
	// 8 bits of Y
	// ... vendor

	uint8_t hat = report_buf[0] & 0x0f;
	uint8_t last_hat = last_report_buf[0] & 0x0f;
	if(hat != last_hat){
		switch(hat){
			case 0:
				// up only
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, -1);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, 0);
				break;
			case 1:
				// up and right
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, -1);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, 1);
				break;
			case 2:
				// right only
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, 0);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, 1);
				break;
			case 3:
				// down right
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, 1);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, 1);
				break;
			case 4:
				// down
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, 1);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, 0);
				break;
			case 5:
				// down left
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, 1);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, -1);
				break;
			case 6:
				// left
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, 0);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, -1);
				break;
			case 7:
				// up left
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, -1);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, -1);
				break;
			case 8:
				// center
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, 0);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, 0);
				break;
		}
	}
	for(int i = 0;i < 22; i++){
		int key = BTN_TRIGGER_HAPPY + i;
		int bit_num = i + 4;
		bool button_on = get_bit(report_buf, bit_num, 12);
		bool button_was_on = get_bit(last_report_buf, bit_num, 12);
		if(button_on != button_was_on){
			EMIT_INPUT(uinput_fd, EV_KEY, key, button_on? 1: 0);
		}
	}
	if(context->combine_pedals == 1){
		report_buf[5] = ((uint32_t)0xFF + report_buf[5] - report_buf[6]) >> 1;
		report_buf[6] = 0x7F;
	}
	if(context->combine_pedals == 2){
		report_buf[5] = ((uint32_t)0xFF + report_buf[5] - report_buf[7]) >> 1;
		report_buf[7] = 0x7F;
	}
	// why don't they align this, what even is a 14bit uint :(
	uint16_t x = 0;
	x = x | report_buf[4] << 6;
	x = x | report_buf[3] >> 2;
	uint16_t last_x = 0;
	last_x = last_x | last_report_buf[4] << 6;
	last_x = last_x | last_report_buf[3] >> 2;
	if(x != last_x){
		EMIT_INPUT(uinput_fd, EV_ABS, ABS_X, x);
	}
	if(report_buf[5] != last_report_buf[5]){
		EMIT_INPUT(uinput_fd, EV_ABS, ABS_Z, report_buf[5]);
	}
	if(report_buf[6] != last_report_buf[6]){
		EMIT_INPUT(uinput_fd, EV_ABS, ABS_RZ, report_buf[6]);
	}
	if(report_buf[7] != last_report_buf[7]){
		EMIT_INPUT(uinput_fd, EV_ABS, ABS_Y, report_buf[7]);
	}

	EMIT_INPUT(uinput_fd, EV_SYN, SYN_REPORT, 0);
	memcpy(last_report_buf, report_buf, 11);
}

static void uinput_dfp_emit(int uinput_fd, uint8_t *report_buf, uint8_t *last_report_buf, struct loop_context *context){
	// 8 byte report
	// first 14 bits of X
	// 14 bits of buttons
	// 4 bits of hat
	// 8 bits of Y
	// 8 bits or Rz

	uint16_t x = 0;
	x = x | report_buf[0];
	x = x | (report_buf[1] & 0x3F) << 8;
	uint16_t last_x = 0;
	last_x = last_x | last_report_buf[0];
	last_x = last_x | (last_report_buf[1] & 0x3F) << 8;
	if(x != last_x){
		EMIT_INPUT(uinput_fd, EV_ABS, ABS_X, x);
	}

	for(int i = 0;i < 14; i++){
		int key = BTN_TRIGGER_HAPPY + i;
		int bit_num = i + 14;
		bool button_on = get_bit(report_buf, bit_num, 8);
		bool button_was_on = get_bit(last_report_buf, bit_num, 8);
		if(button_on != button_was_on){
			EMIT_INPUT(uinput_fd, EV_KEY, key, button_on? 1: 0);
		}
	}

	uint8_t hat = report_buf[3] >> 4;
	uint8_t last_hat = last_report_buf[3] >> 4;
	if(hat != last_hat){
		switch(hat){
			case 0:
				// up only
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, -1);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, 0);
				break;
			case 1:
				// up and right
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, -1);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, 1);
				break;
			case 2:
				// right only
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, 0);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, 1);
				break;
			case 3:
				// down right
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, 1);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, 1);
				break;
			case 4:
				// down
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, 1);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, 0);
				break;
			case 5:
				// down left
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, 1);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, -1);
				break;
			case 6:
				// left
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, 0);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, -1);
				break;
			case 7:
				// up left
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, -1);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, -1);
				break;
			case 8:
				// center
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0Y, 0);
				EMIT_INPUT(uinput_fd, EV_ABS, ABS_HAT0X, 0);
				break;
		}
	}

	if(context->combine_pedals == 1){
		report_buf[5] = ((uint32_t)0xFF + report_buf[5] - report_buf[6]) >> 1;
		report_buf[6] = 0x7F;
	}
	if(report_buf[5] != last_report_buf[5]){
		EMIT_INPUT(uinput_fd, EV_ABS, ABS_Y, report_buf[5]);
	}
	if(report_buf[6] != last_report_buf[6]){
		EMIT_INPUT(uinput_fd, EV_ABS, ABS_RZ, report_buf[6]);
	}

	EMIT_INPUT(uinput_fd, EV_SYN, SYN_REPORT, 0);
	memcpy(last_report_buf, report_buf, 8);
}

struct input_loop_context{
	struct loop_context context;
	hid_device *read_hid_device;
	int uinput_fd;
	pthread_mutex_t *uinput_write_mutex;
};

static void *input_loop(void *arg){
	struct input_loop_context *loop_context = (struct input_loop_context *)arg;
	uint8_t report_buf[32] = {0};
	uint8_t last_report_buf[32] = {0};
	while(true){
		switch(loop_context->context.device.product_id){
			case USB_DEVICE_ID_LOGITECH_G29_WHEEL:{
				int bytes_read = hid_read(loop_context->read_hid_device, report_buf, 12);
				if(bytes_read == -1){
					char error_buf[128];
					wcstombs(error_buf, hid_error(loop_context->read_hid_device), sizeof(error_buf));
					STDERR("failed reading input report from G29, %s\n", error_buf);
					exit(1);
				}
				pthread_mutex_lock(loop_context->uinput_write_mutex);
				uinput_g29_emit(loop_context->uinput_fd, report_buf, last_report_buf, &loop_context->context);
				pthread_mutex_unlock(loop_context->uinput_write_mutex);
				break;
			}
			case USB_DEVICE_ID_LOGITECH_G27_WHEEL:
			case USB_DEVICE_ID_LOGITECH_G25_WHEEL:{
				int bytes_read = hid_read(loop_context->read_hid_device, report_buf, 11);
				if(bytes_read == -1){
					char error_buf[128];
					wcstombs(error_buf, hid_error(loop_context->read_hid_device), sizeof(error_buf));
					STDERR("failed reading input report from G25/G27, %s\n", error_buf);
					exit(1);
				}
				pthread_mutex_lock(loop_context->uinput_write_mutex);
				uinput_g27_emit(loop_context->uinput_fd, report_buf, last_report_buf, &loop_context->context);
				pthread_mutex_unlock(loop_context->uinput_write_mutex);
				break;
			}
			case USB_DEVICE_ID_LOGITECH_DFP_WHEEL:{
				int bytes_read = hid_read(loop_context->read_hid_device, report_buf, 8);
				if(bytes_read == -1){
					char error_buf[128];
					wcstombs(error_buf, hid_error(loop_context->read_hid_device), sizeof(error_buf));
					STDERR("failed reading input report from Drive Force Pro, %s\n", error_buf);
					exit(1);
				}
				pthread_mutex_lock(loop_context->uinput_write_mutex);
				uinput_dfp_emit(loop_context->uinput_fd, report_buf, last_report_buf, &loop_context->context);
				pthread_mutex_unlock(loop_context->uinput_write_mutex);
				break;
			}
			default:
				STDERR("uinput device for %s(0x%04x) is not implemented\n", get_name_by_product_id(loop_context->context.device.product_id), loop_context->context.device.product_id);
				exit(1);
		}
	}
}

struct output_loop_context{
	struct loop_context context;
	int uinput_fd;
	pthread_mutex_t *uinput_write_mutex;
	hid_device *write_hid_device;
	pthread_mutex_t device_mutex;
	struct lg4ff_device ffb_device;
};

#define CLAMP_VALUE(v, min, max){ \
	if(v > max){ \
		v = max; \
	} \
	if(v < min){ \
		v = min; \
	} \
}

static void *effect_loop(void *arg){
	struct output_loop_context *loop_context = (struct output_loop_context *)arg;
	bool effect_was_playing[LG4FF_MAX_EFFECTS] = {0};
	bool effect_playing[LG4FF_MAX_EFFECTS] = {0};
	while(true){
		pthread_mutex_lock(&loop_context->device_mutex);
		lg4ff_timer(&loop_context->ffb_device);

		for(int i = 0;i < LG4FF_MAX_EFFECTS; i++){
			effect_playing[i] = (loop_context->ffb_device.states[i].flags & (1 << FF_EFFECT_PLAYING))? true: false;
		}
		pthread_mutex_unlock(&loop_context->device_mutex);

		for(int i = 0; i < LG4FF_MAX_EFFECTS; i++){
			if(effect_playing[i] != effect_was_playing[i]){
				effect_was_playing[i] = effect_playing[i];

				EMIT_INPUT(loop_context->uinput_fd, EV_FF_STATUS, i, effect_playing[i]? FF_STATUS_PLAYING :FF_STATUS_STOPPED);
				EMIT_INPUT(loop_context->uinput_fd, EV_SYN, SYN_REPORT, 0);
				pthread_mutex_unlock(loop_context->uinput_write_mutex);
			}
		}

		struct timespec duration = {0};
		duration.tv_nsec = 2 * 1000000;
		nanosleep(&duration, NULL);
	}
}

static void *uinput_poll_loop(void *arg){
	struct output_loop_context *loop_context = (struct output_loop_context *)arg;
	int ret;
	while(true){
		struct input_event e;
		int ret = read(loop_context->uinput_fd, &e, sizeof(struct input_event));
		if(ret != sizeof(struct input_event)){
			STDERR("failed reading ffb event from uinput, %s\n", strerror(errno));
			exit(1);
		}

		switch(e.type){
			case EV_FF:{
				switch(e.code){
					case FF_AUTOCENTER:{
						CLAMP_VALUE(e.value, 0, 65535);
						pthread_mutex_lock(&loop_context->device_mutex);
						set_auto_center(loop_context->write_hid_device, loop_context->context.device.product_id, e.value);
						pthread_mutex_unlock(&loop_context->device_mutex);
						break;
					}
					case FF_GAIN:{
						CLAMP_VALUE(e.value, 0, 65535);
						pthread_mutex_lock(&loop_context->device_mutex);
						loop_context->ffb_device.app_gain = e.value;
						pthread_mutex_unlock(&loop_context->device_mutex);
						break;
					}
					default:{
						pthread_mutex_lock(&loop_context->device_mutex);
						lg4ff_play_effect(&loop_context->ffb_device, e.code, e.value);
						pthread_mutex_unlock(&loop_context->device_mutex);
					}
				}
				break;
			}
			case EV_UINPUT:{
				switch(e.code){
					case UI_FF_UPLOAD:{
						struct uinput_ff_upload upload;
						upload.request_id = e.value;
						ioctl(loop_context->uinput_fd, UI_BEGIN_FF_UPLOAD, &upload);
						pthread_mutex_lock(&loop_context->device_mutex);
						upload.retval = lg4ff_upload_effect(&loop_context->ffb_device, &upload.effect, &upload.old);
						if(loop_context->context.play_on_upload && upload.retval == 0){
							lg4ff_play_effect(&loop_context->ffb_device, upload.effect.id, 1);
						}
						pthread_mutex_unlock(&loop_context->device_mutex);
						ioctl(loop_context->uinput_fd, UI_END_FF_UPLOAD, &upload);
						break;
					}
					case UI_FF_ERASE:{
						struct uinput_ff_erase erase;
						erase.request_id = e.value;
						ioctl(loop_context->uinput_fd, UI_BEGIN_FF_ERASE, &erase);
						// not implemented
						erase.retval = 0;
						ioctl(loop_context->uinput_fd, UI_END_FF_ERASE, &erase);
						break;
					}
				}
			}
		}

	}
}

void start_loops(struct loop_context context){
	hid_device *device = hid_open_path(context.device.backend_path);
	if(device == NULL){
		char error_buf[128];
		wcstombs(error_buf, hid_error(NULL), sizeof(error_buf));
		STDERR("failed opening device on backend path %s, %s\n", context.device.backend_path, error_buf);
		exit(1);
	}

	// register uinput device for reading and writing
	int uinput_fd = open("/dev/uinput", O_RDWR);
	if(uinput_fd < 0){
		STDERR("failed opening /dev/uinput, %s\n", strerror(errno));
		exit(1);
	}

	switch(context.device.product_id){
		case USB_DEVICE_ID_LOGITECH_G29_WHEEL:
			uinput_g29_setup(uinput_fd, &context);
			break;
		case USB_DEVICE_ID_LOGITECH_G27_WHEEL:
			uinput_g27_setup(uinput_fd, &context);
			break;
		case USB_DEVICE_ID_LOGITECH_G25_WHEEL:
			uinput_g25_setup(uinput_fd, &context);
			break;
		case USB_DEVICE_ID_LOGITECH_DFP_WHEEL:
			uinput_dfp_setup(uinput_fd, &context);
			break;
		default:
			STDERR("uinput device for %s(0x%04x) is not implemented\n", get_name_by_product_id(context.device.product_id), context.device.product_id);
			exit(1);
	}

	pthread_mutex_t uinput_write_mutex;
	int ret = pthread_mutex_init(&uinput_write_mutex, NULL);
	if(ret != 0){
		STDERR("failed creating uinput write mutex\n");
		exit(1);
	}

	set_range(device, context.device.product_id, context.range);
	set_auto_center(device, context.device.product_id, context.auto_center);

	struct input_loop_context ilc = {
		.context = context,
		.read_hid_device = device,
		.uinput_fd = uinput_fd,
		.uinput_write_mutex = &uinput_write_mutex
	};
	pthread_t input_thread;
	ret = pthread_create(&input_thread, NULL, input_loop, (void *)&ilc);
	if(ret != 0){
		STDERR("failed starting input polling thread\n");
		exit(1);
	}

	struct output_loop_context olc = {0};
	olc.context = context;
	olc.uinput_fd = uinput_fd;
	olc.uinput_write_mutex = &uinput_write_mutex;
	olc.write_hid_device = device;
	olc.ffb_device.effects_used = 0;
	olc.ffb_device.gain = context.gain;
	olc.ffb_device.app_gain = 0xffff;
	olc.ffb_device.spring_level = context.spring_level;
	olc.ffb_device.damper_level = context.damper_level;
	olc.ffb_device.friction_level = context.friction_level;
	olc.ffb_device.hid_handle = device;

	lg4ff_init_slots(&olc.ffb_device);

	ret = pthread_mutex_init(&olc.device_mutex, NULL);
	if(ret != 0){
		STDERR("failed initializing mutex for ffb device loops\n");
		exit(1);
	}

	pthread_t uinput_poll_thread;
	ret = pthread_create(&uinput_poll_thread, NULL, uinput_poll_loop, (void *)&olc);
	if(ret != 0){
		STDERR("failed starting uinput polling thread\n");
		exit(1);
	}

	pthread_t effect_thread;
	ret = pthread_create(&effect_thread, NULL, effect_loop, (void *)&olc);
	if(ret != 0){
		STDERR("failed starting effect thread, %s\n", strerror(ret));
		exit(1);
	}
	pthread_join(effect_thread, NULL);

	pthread_join(input_thread, NULL);
	pthread_join(uinput_poll_thread, NULL);
}

