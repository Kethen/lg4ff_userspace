#ifndef __CONFIGURE_H
#define __CONFIGURE_H

int set_range(hid_device *hd, uint16_t product_id, int range);
int set_auto_center(hid_device *hd, uint16_t product_id, int gain);

#endif
