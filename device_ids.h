#ifndef __DEVICE_IDS_H
#define __DEVICE_IDS_H

#include <stdint.h>

#define USB_VENDOR_ID_LOGITECH 0x046d
#define USB_DEVICE_ID_LOGITECH_G29_WHEEL 0xc24f
#define USB_DEVICE_ID_LOGITECH_G27_WHEEL 0xc29b
#define USB_DEVICE_ID_LOGITECH_G25_WHEEL 0xc299
#define USB_DEVICE_ID_LOGITECH_DFGT_WHEEL 0xc29a
#define USB_DEVICE_ID_LOGITECH_DFP_WHEEL 0xc298
#define USB_DEVICE_ID_LOGITECH_WHEEL 0xc294

static uint32_t supported_ids[] = {
	USB_DEVICE_ID_LOGITECH_G29_WHEEL,
	USB_DEVICE_ID_LOGITECH_G27_WHEEL,
	USB_DEVICE_ID_LOGITECH_G25_WHEEL,
	USB_DEVICE_ID_LOGITECH_DFGT_WHEEL,
	USB_DEVICE_ID_LOGITECH_DFP_WHEEL,
	USB_DEVICE_ID_LOGITECH_WHEEL
};

const char *get_name_by_product_id(int id);

#endif
