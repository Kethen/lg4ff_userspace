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

#define SETUP_KEY(f, k){ \
	ioctl(f, UI_SET_EVBIT, EV_KEY); \
	ioctl(f, UI_SET_KEYBIT, k); \
}

#define SETUP_AXIS(f, a, min, max){ \
	ioctl(f, UI_SET_EVBIT, EV_ABS); \
	ioctl(f, UI_SET_ABSBIT, a); \
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

static void uinput_g29_setup(int uinput_fd){
	// 25 buttons numerated from 1 to 25 on hid
	// wine just goes ascending so map all 25 onto trigger happy during evdev -> hid
	for(int i = 0;i < 25; i++){
		int key = BTN_TRIGGER_HAPPY + i;
		SETUP_KEY(uinput_fd, key);
	}

	// X axis on 0-65535, wine seems to convert them by name during evdev -> hid
	SETUP_AXIS(uinput_fd, ABS_X, 0, 65535);
	// Z on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_Z, 0, 255);
	// Rz on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_RZ, 0, 255);
	// Y on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_Y, 0, 255);

	// HAT/dpad
	SETUP_AXIS(uinput_fd, ABS_HAT0X, -1, 1);
	SETUP_AXIS(uinput_fd, ABS_HAT0Y, -1, 1);

	struct uinput_setup usetup = {0};
	usetup.id.bustype = BUS_USB;
	usetup.id.vendor = USB_VENDOR_ID_LOGITECH;
	usetup.id.product = USB_DEVICE_ID_LOGITECH_G29_WHEEL;
	strcpy(usetup.name, "userspace G29");

	ioctl(uinput_fd, UI_DEV_SETUP, &usetup);
	ioctl(uinput_fd, UI_DEV_CREATE);
}

static void uinput_g27_setup(int uinput_fd){
	// 22 buttons numerated from 1 to 22 on hid
	for(int i = 0;i < 22; i++){
		int key = BTN_TRIGGER_HAPPY + i;
		SETUP_KEY(uinput_fd, key);
	}

	// X axis on 0-16383, wine seems to convert them by name during evdev -> hid
	SETUP_AXIS(uinput_fd, ABS_X, 0, 16383);
	// Z on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_Z, 0, 255);
	// Rz on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_RZ, 0, 255);
	// Y on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_Y, 0, 255);

	// HAT/dpad
	SETUP_AXIS(uinput_fd, ABS_HAT0X, -1, 1);
	SETUP_AXIS(uinput_fd, ABS_HAT0Y, -1, 1);

	struct uinput_setup usetup = {0};
	usetup.id.bustype = BUS_USB;
	usetup.id.vendor = USB_VENDOR_ID_LOGITECH;
	usetup.id.product = USB_DEVICE_ID_LOGITECH_G27_WHEEL;
	strcpy(usetup.name, "userspace G27");

	ioctl(uinput_fd, UI_DEV_SETUP, &usetup);
	ioctl(uinput_fd, UI_DEV_CREATE);
}

static void uinput_g25_setup(int uinput_fd){
	// 22 buttons numerated from 1 to 22 on hid
	for(int i = 0;i < 19; i++){
		int key = BTN_TRIGGER_HAPPY + i;
		SETUP_KEY(uinput_fd, key);
	}

	// X axis on 0-16383, wine seems to convert them by name during evdev -> hid
	SETUP_AXIS(uinput_fd, ABS_X, 0, 16383);
	// Z on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_Z, 0, 255);
	// Rz on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_RZ, 0, 255);
	// Y on 255 - 0
	SETUP_AXIS(uinput_fd, ABS_Y, 0, 255);

	// HAT/dpad
	SETUP_AXIS(uinput_fd, ABS_HAT0X, -1, 1);
	SETUP_AXIS(uinput_fd, ABS_HAT0Y, -1, 1);

	struct uinput_setup usetup = {0};
	usetup.id.bustype = BUS_USB;
	usetup.id.vendor = USB_VENDOR_ID_LOGITECH;
	usetup.id.product = USB_DEVICE_ID_LOGITECH_G25_WHEEL;
	strcpy(usetup.name, "userspace G25");

	ioctl(uinput_fd, UI_DEV_SETUP, &usetup);
	ioctl(uinput_fd, UI_DEV_CREATE);
}

static void uinput_g29_emit(int uinput_fd, uint8_t *report_buf, uint8_t *last_report_buf){
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

static void uinput_g27_emit(int uinput_fd, uint8_t *report_buf, uint8_t *last_report_buf){
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

struct input_loop_context{
	struct loop_context context;
	hid_device *read_hid_device;
	int uinput_fd;
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
				uinput_g29_emit(loop_context->uinput_fd, report_buf, last_report_buf);
				break;
			}
			case USB_DEVICE_ID_LOGITECH_G27_WHEEL:
			case USB_DEVICE_ID_LOGITECH_G25_WHEEL:{
				int bytes_read = hid_read(loop_context->read_hid_device, report_buf, 11);
				if(bytes_read == -1){
					char error_buf[128];
					wcstombs(error_buf, hid_error(loop_context->read_hid_device), sizeof(error_buf));
					STDERR("failed reading input report from G27, %s\n", error_buf);
					exit(1);
				}
				uinput_g27_emit(loop_context->uinput_fd, report_buf, last_report_buf);
				break;
			}
			default:
				STDERR("uinput device for %s(0x%04x) is not implemented\n", get_name_by_product_id(loop_context->context.device.product_id), loop_context->context.device.product_id);
				exit(1);
		}
	}
}

void start_loops(struct loop_context context){
	// open hid device twice, once for input, once for output
	hid_device *read_hid_device = hid_open_path(context.device.backend_path);
	if(read_hid_device == NULL){
		char error_buf[128];
		wcstombs(error_buf, hid_error(NULL), sizeof(error_buf));
		STDERR("failed opening hidraw device on backend path %s for reading, %s\n", context.device.backend_path, error_buf);
		exit(1);
	}

	hid_device *write_hid_device = hid_open_path(context.device.backend_path);
	if(write_hid_device == NULL){
		char error_buf[128];
		wcstombs(error_buf, hid_error(NULL), sizeof(error_buf));
		STDERR("failed opening hidraw device on backend path %s for writing, %s\n", context.device.backend_path, error_buf);
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
			uinput_g29_setup(uinput_fd);
			break;
		case USB_DEVICE_ID_LOGITECH_G27_WHEEL:
			uinput_g27_setup(uinput_fd);
			break;
		case USB_DEVICE_ID_LOGITECH_G25_WHEEL:
			uinput_g25_setup(uinput_fd);
			break;
		default:
			STDERR("uinput device for %s(0x%04x) is not implemented\n", get_name_by_product_id(context.device.product_id), context.device.product_id);
			exit(1);
	}


	set_range(write_hid_device, context.device.product_id, context.range);
	set_auto_center(write_hid_device, context.device.product_id, context.auto_center);

	struct input_loop_context ilc = {
		.context = context,
		.read_hid_device = read_hid_device,
		.uinput_fd = uinput_fd
	};
	pthread_t input_thread;
	int ret = pthread_create(&input_thread, NULL, input_loop, (void *)&ilc);
	if(ret != 0){
		STDERR("failed starting input polling thread\n");
		exit(1);
	}

	// TODO ffb loop


	pthread_join(input_thread, NULL);
}

