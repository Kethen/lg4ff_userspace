#ifndef __LOGGING_H
#define __LOGGING_H

#include <stdio.h>

#define STDOUT(...){ \
	fprintf(stdout, __VA_ARGS__); \
}

#define STDERR(...){ \
	fprintf(stderr, __VA_ARGS__); \
}

#endif
