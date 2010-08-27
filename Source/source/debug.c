
// by oggzee

#include <gccore.h>
#include <ogcsys.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>

#include "cfg.h"

extern long long gettime();
extern u32 diff_msec(long long start, long long end);


int gecko_enabled = 0;

static long long dbg_t1, dbg_t2;


void dbg_time1()
{
	dbg_t1 = gettime();
}

unsigned dbg_time2(char *msg)
{
	unsigned ms;
	dbg_t2 = gettime();
	ms = diff_msec(dbg_t1, dbg_t2);
	if (msg) printf("%s %u", msg, ms);
	dbg_t1 = dbg_t2;
	return ms;
}

int dbg_printf(const char *fmt, ... )
{
	char str[1024];
	va_list ap;
	int r = 0;

	if (CFG.debug) {
		va_start(ap, fmt);
		r = vprintf(fmt, ap);
		va_end(ap);
	}
	if (gecko_enabled) {
		va_start(ap, fmt);
		r = vsprintf(str, fmt, ap);
		va_end(ap);
		usb_sendbuffer(EXI_CHANNEL_1, str, strlen(str));
	}
	return r;
}

void InitDebug()
{
	if (CFG.debug_gecko & 1)
	{
		if (usb_isgeckoalive(EXI_CHANNEL_1)) {
			usb_flush(EXI_CHANNEL_1);
			gecko_enabled = 1;
		}
	}
	if (CFG.debug_gecko & 2) {
		CON_EnableGecko(EXI_CHANNEL_1, 0);
	}
}


