#ifndef __LOGGING_H
#define __LOGGING_H

#include <stdio.h>
#include <unistd.h>

#define STDOUT(...){ \
	char _log_buf[512]; \
	int _log_len = sprintf(_log_buf, __VA_ARGS__); \
	write(1, _log_buf, _log_len); \
}

#define STDERR(...){ \
	char _log_buf[512]; \
	int _log_len = sprintf(_log_buf, __VA_ARGS__); \
	write(2, _log_buf, _log_len); \
}

#endif
