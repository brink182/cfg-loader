#ifndef _HTTP_H_
#define _HTTP_H_

#include <errno.h>
#include <network.h>
#include <ogcsys.h>
#include <string.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "dns.h"

/**
 * A simple structure to keep track of the size of a malloc()ated block of memory 
 */
struct block
{
	u32 size;
	unsigned char *data;
};

extern const struct block emptyblock;

struct block downloadfile(const char *url);

#endif /* _HTTP_H_ */
