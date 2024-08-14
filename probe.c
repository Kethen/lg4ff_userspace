#include <string.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <linux/hidraw.h>

#include <sys/ioctl.h>

#include "logging.h"
#include "device_ids.h"

#include "probe.h"

int find_wheels(struct hidraw_device *hidraw_devices, int buffer_size){
	DIR *dev_dir = opendir("/dev");
	if(dev_dir == NULL){
		STDERR("cannot open /dev\n");
		return 0;
	}

	int wheels_found = 0;

	while(true){
		struct dirent *file_entry = readdir(dev_dir);
		if(file_entry == NULL){
			break;
		}

		if(wheels_found == buffer_size){
			break;
		}

		if(file_entry->d_type != DT_CHR){
			continue;
		}
		if(strlen(file_entry->d_name) <= 6){
			continue;
		}
		if(memcmp("hidraw", file_entry->d_name, 6) != 0){
			continue;
		}

		char path_buf[128];
		sprintf(path_buf, "/dev/%s", file_entry->d_name);
		int fd = open(path_buf, O_RDWR);
		if(fd < 0){
			STDERR("cannot open %s for ioctl, %d\n", path_buf, errno);
			continue;
		}

		struct hidraw_devinfo info;
		int res = ioctl(fd, HIDIOCGRAWINFO, &info);
		if(res < 0){
			STDERR("cannot fetch device information from %s\n", path_buf);
			close(fd);
			continue;
		}


		if((uint16_t)info.vendor != (uint16_t)USB_VENDOR_ID_LOGITECH){
			close(fd);
			continue;
		}
		int i;
		for(i = 0;i < sizeof(supported_ids) / sizeof(int); i++){
			if((int16_t)info.product == (int16_t)supported_ids[i]){
				strcpy(hidraw_devices[wheels_found].path_buf, path_buf);
				ioctl(fd, HIDIOCGRAWNAME(256), hidraw_devices[wheels_found].name_buf);
				hidraw_devices[wheels_found].vendor_id = (uint16_t)info.vendor;
				hidraw_devices[wheels_found].product_id = (uint16_t)info.product;
				wheels_found++;
				break;
			}
		}
		close(fd);
	}
	closedir(dev_dir);
	return wheels_found;
}
