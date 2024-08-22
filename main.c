#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>

#include "logging.h"
#include "probe.h"
#include "driver_loops.h"
#include "switch_mode.h"
#include "device_ids.h"

static void list_devices(struct hidraw_device *hidraw_devices, int entries){
	for(int i = 0;i < entries; i++){
		STDOUT("Device %d:\n", i + 1);
		STDOUT(" name: %s\n", hidraw_devices[i].name_buf);
		STDOUT(" vendor id: 0x%04x\n", hidraw_devices[i].vendor_id);
		STDOUT(" product id: 0x%04x\n", hidraw_devices[i].product_id);
		STDOUT(" mode: %s\n", get_name_by_product_id(hidraw_devices[i].product_id));
		STDOUT(" backend path: %s\n", hidraw_devices[i].backend_path);
	}
}

static void print_help(char *binary_name){
	STDOUT("usage: %s <options>\n", binary_name);
	STDOUT("listing devices:\n");
	STDOUT("  %s -l\n", binary_name)
	STDOUT("display this message:\n");
	STDOUT("  %s -h\n", binary_name);
	STDOUT("reboot wheel into another mode:\n");
	STDOUT("  %s -m <dfp/g25/g27/g29> [-n device number in -l]\n", binary_name);
	STDOUT("start driver on wheel:\n");
	STDOUT("  %s -w [driver options]\n", binary_name);
	STDOUT("  driver options:\n");
	STDOUT("  wheel number to start the driver on, defaults to 1:\n")
	STDOUT("    [-n <device number in -l>]\n");
	STDOUT("  gain, independent of application set gain, defaults to 65535:\n");
	STDOUT("    [-g <gain, 0-65535>]\n");
	STDOUT("  auto center gain, can be overriden by application, defaults to 0:\n")
	STDOUT("    [-a <auto center, 0-65535>]\n");
	STDOUT("  spring effect level, defaults to 30:\n")
	STDOUT("    [-s <spring level, 0-100>]\n");
	STDOUT("  damper effect level, defaults to 30:\n")
	STDOUT("    [-d <damper level, 0-100>]\n");
	STDOUT("  friction effect level, defaults to 30:\n")
	STDOUT("    [-f <friction level, 0-100>]\n");
	STDOUT("  hide all effects except for constant force from application:\n");
	STDOUT("    [-H]\n");
	STDOUT("  combine pedals, 0 for not combining any, 1 for combining gas and brake, 2 for combining gas and clutch, defaults to 0:\n");
	STDOUT("    [-c <0/1/2>]\n");
	STDOUT("  play effect on upload, for 'Fast' update type in BeamNG.drive:\n");
	STDOUT("    [-u]\n")
}

enum operation_mode{
	OPERATION_MODE_REBOOT = 0,
	OPERATION_MODE_DRIVER
};

enum wheel_mode{
	WHEEL_MODE_DFP = 0,
	WHEEL_MODE_G25,
	WHEEL_MODE_G27,
	WHEEL_MODE_G29
};

static const uint16_t get_wheel_mode_product_id(int mode){
	switch(mode){
		case WHEEL_MODE_DFP:
			return USB_DEVICE_ID_LOGITECH_DFP_WHEEL;
		case WHEEL_MODE_G25:
			return USB_DEVICE_ID_LOGITECH_G25_WHEEL;
		case WHEEL_MODE_G27:
			return USB_DEVICE_ID_LOGITECH_G27_WHEEL;
		case WHEEL_MODE_G29:
			return USB_DEVICE_ID_LOGITECH_G29_WHEEL;
	}
	STDERR("unknown wheel mode %d\n", mode);
	exit(1);
	return 0;
}

static const char *get_wheel_mode_name(int mode){
	int product_id = get_wheel_mode_product_id(mode);
	return get_name_by_product_id(product_id);
}

static int reboot_wheel(struct hidraw_device *hidraw_devices, int wheels, int wheel_num, int target_mode){
	list_devices(hidraw_devices, wheels);
	STDOUT("rebooting wheel %d to %s\n", wheel_num, get_wheel_mode_name(target_mode));
	switch_mode(hidraw_devices[wheel_num - 1], get_wheel_mode_product_id(target_mode));
	return 0;
}

static int start_driver(
		struct hidraw_device *hidraw_devices,
		int wheels,
		int wheel_num,
		int gain,
		int auto_center,
		int spring_level,
		int damper_level,
		int friction_level,
		int range,
		bool hide_effects,
		int combine_pedals,
		bool play_on_upload){
	list_devices(hidraw_devices, wheels);
	STDOUT("starting driver on wheel %d\n", wheel_num);
	STDOUT("gain: %d\n", gain);
	STDOUT("auto center: %d\n", auto_center);
	STDOUT("spring level: %d\n", spring_level);
	STDOUT("damper level: %d\n", damper_level);
	STDOUT("friction level: %d\n", friction_level);
	STDOUT("range: %d\n", range);
	STDOUT("hide effects: %s\n", hide_effects? "true": "false");
	STDOUT("combine pedals: %d\n", combine_pedals);
	STDOUT("play effect on upload: %s\n", play_on_upload? "true" : "false");

	struct loop_context lc = {
		.device = hidraw_devices[wheel_num - 1],
		.gain = gain,
		.auto_center = auto_center,
		.spring_level = spring_level,
		.damper_level = damper_level,
		.friction_level = friction_level,
		.range = range,
		.hide_effects = hide_effects,
		.combine_pedals = combine_pedals,
		.play_on_upload = play_on_upload
	};
	start_loops(lc);

	return 0;
}

int main(int argc, char** argv){
	struct hidraw_device hidraw_devices[256];
	int wheels_found = find_wheels(hidraw_devices, sizeof(hidraw_devices) / sizeof(struct hidraw_device));

	if(argc == 1){
		print_help(argv[0]);
		return 0;
	}

	int opt;
	int mode = -1;
	int wmode = -1;
	int device_number = 1;
	int gain = 65535;
	int auto_center = 0;
	int spring_level = 30;
	int damper_level = 30;
	int friction_level = 30;
	int range = 900;
	bool hide_effects = false;
	int combine_pedals = 0;
	bool play_on_upload = false;

	while ((opt = getopt(argc, argv, "lhm:n:wg:a:s:d:f:r:Hc:u")) != -1){
		#define CLAMP_ARG_VALUE(name, field, min, max){ \
			if(field > max){ \
				field = max; \
				STDERR("%s is too large, setting %d instead\n", name, max); \
			} \
			if(field < min){ \
				field = min; \
				STDERR("%s is too small, setting %d instead\n", name, min); \
			} \
		}

		switch(opt){
			case 'l':
				list_devices(hidraw_devices, wheels_found);
				return 0;
				break;
			case 'm':
				mode = OPERATION_MODE_REBOOT;
				if(strcmp(optarg, "dfp") == 0){
					wmode = WHEEL_MODE_DFP;
				}else if(strcmp(optarg, "g25") == 0){
					wmode = WHEEL_MODE_G25;
				}else if(strcmp(optarg, "g27") == 0){
					wmode = WHEEL_MODE_G27;
				}else if(strcmp(optarg, "g29") == 0){
					wmode = WHEEL_MODE_G29;
				}
				break;
			case 'n':
				device_number = atoi(optarg);
				break;
			case 'w':
				mode = OPERATION_MODE_DRIVER;
				break;
			case 'g':
				gain = atoi(optarg);
				CLAMP_ARG_VALUE("gain", gain, 0, 65535);
				break;
			case 'a':
				auto_center = atoi(optarg);
				CLAMP_ARG_VALUE("auto center", auto_center, 0, 65535);
				break;
			case 's':
				spring_level = atoi(optarg);
				CLAMP_ARG_VALUE("spring level", spring_level, 0, 100);
				break;
			case 'd':
				damper_level = atoi(optarg);
				CLAMP_ARG_VALUE("damper level", damper_level, 0, 100);
				break;
			case 'f':
				friction_level = atoi(optarg);
				CLAMP_ARG_VALUE("friction level", friction_level, 0, 100);
				break;
			case 'r':
				range = atoi(optarg);
				break;
			case 'H':
				hide_effects = true;
				break;
			case 'c':
				combine_pedals = atoi(optarg);
				CLAMP_ARG_VALUE("combine pedals", combine_pedals, 0, 2);
				break;
			case 'u':
				play_on_upload = true;
				break;
			default:
				print_help(argv[0]);
				return 0;
		}

		#undef CLAMP_ARG_VALUE
	}


	if(wheels_found == 0){
		STDERR("no supported wheel found\n");
		return 0;
	}

	if(device_number < 1 || device_number > wheels_found){
		STDERR("invalid device number %d, there %s only %d %s\n", device_number, wheels_found > 1? "are" : "is", wheels_found, wheels_found > 1? "wheels" : "wheel");
		list_devices(hidraw_devices, wheels_found);
		return 0;
	}

	switch(mode){
		case OPERATION_MODE_REBOOT:
			reboot_wheel(hidraw_devices, wheels_found, device_number, wmode);
			return 0;
		case OPERATION_MODE_DRIVER:
			start_driver(hidraw_devices, wheels_found, device_number, gain, auto_center, spring_level, damper_level, friction_level, range, hide_effects, combine_pedals, play_on_upload);
			return 0;
		default:
			print_help(argv[0]);
			return 0;
	}

    return 0;
}
