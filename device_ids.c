#include "device_ids.h"

const char *get_name_by_product_id(int id){
	switch(id){
		case USB_DEVICE_ID_LOGITECH_G29_WHEEL:
			return "G29";
		case USB_DEVICE_ID_LOGITECH_G27_WHEEL:
			return "G27";
		case USB_DEVICE_ID_LOGITECH_G25_WHEEL:
			return "G25";
		case USB_DEVICE_ID_LOGITECH_DFGT_WHEEL:
			return "Driving Force GT";
		case USB_DEVICE_ID_LOGITECH_DFP_WHEEL:
			return "Driving Force Pro";
		case USB_DEVICE_ID_LOGITECH_WHEEL:
			return "Driving Force EX";
	}
	return "unknown";
}
