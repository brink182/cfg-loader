
// by oggzee

#include <gccore.h>
#include <ogcsys.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>

#include "cfg.h"
#include "console.h"

extern long long gettime();
extern u32 diff_msec(long long start, long long end);

struct timestats TIME;

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

int gecko_printv(const char *fmt, va_list ap)
{
	char str[1024];
	int r;
	if (!gecko_enabled) return 0;
	r = vsnprintf(str, sizeof(str), fmt, ap);
	usb_sendbuffer(EXI_CHANNEL_1, str, strlen(str));
	return r;
}

int gecko_printf(const char *fmt, ... )
{
	va_list ap;
	int r;
	if (!gecko_enabled) return 0;
	va_start(ap, fmt);
	r = gecko_printv(fmt, ap);
	va_end(ap);
	return r;
}

int dbg_printf(const char *fmt, ... )
{
	va_list ap;
	int r = 0;

	if (CFG.debug) {
		va_start(ap, fmt);
		r = vprintf(fmt, ap);
		va_end(ap);
	}
	if (gecko_enabled && (CFG.debug_gecko & 1)) {
		va_start(ap, fmt);
		r = gecko_printv(fmt, ap);
		va_end(ap);
	}
	return r;
}

int dbg_print(int level, const char *fmt, ...)
{
	va_list ap;
	int r = 0;
	bool dl = CFG.debug >= level;
	if (!dl && level > 1) return r;
	if (dl) {
		va_start(ap, fmt);
		r = vprintf(fmt, ap);
		va_end(ap);
	}
	if (gecko_enabled && (CFG.debug_gecko & 1)) {
		va_start(ap, fmt);
		r = gecko_printv(fmt, ap);
		va_end(ap);
	}
	return r;
}

/*
debug_gecko bits:
1 : all dbg_printf
2 : all printf
4 : only gecko_printf
*/

void InitDebug()
{
	if (CFG.debug_gecko & (1|4))
	{
		if (usb_isgeckoalive(EXI_CHANNEL_1)) {
			usb_flush(EXI_CHANNEL_1);
			if (!gecko_enabled) {
				gecko_enabled = 1;
				gecko_printf("\n\n====================\n\n");
			}
		}
	}
	if (CFG.debug_gecko & 2) {
		CON_EnableGecko(EXI_CHANNEL_1, 0);
	}
}

/* seconds in float */
float diff_fsec(long long t1, long long t2)
{
	return (float)diff_msec(t1, t2) / 1000.0;
}

void get_time2(long long *t, char *s)
{
	if (!t) return;
	if (*t == 0) *t = gettime();
	if (CFG.debug || CFG.debug_gecko) {
		char c;
		char *p;
		char ss[32];
		char *dir;
		int len;
		long long tt = gettime();
		p = strchr(s, '.');
		if (p) p++; else p = s;
		STRCOPY(ss, p);
		len = strlen(ss) - 1;
		c = ss[len];
		if (c == '1') { dir = "-->"; ss[len] = 0; }
		else if (c == '2') { dir = "<--"; ss[len] = 0; }
		else return; //dir = "---";
		dbg_printf("[%.3f] %s %s\n", diff_fsec(TIME.boot1, tt), dir, ss);
		__console_flush(0);
	}
}

long tm_sum;

#define TIME_S(X) ((float)TIME_MS(X)/1000.0)

#define printx(...) do{ \
	printf(__VA_ARGS__); \
	if(f)fprintf(f,__VA_ARGS__); \
   	gecko_printf(__VA_ARGS__); \
    }while(0)

#define print_t3(S,X,Y) do{\
	tm_sum += TIME_MS(X);\
	printx("%s: %.3f%s", S, TIME_S(X),Y);\
	}while(0)

#define print_t2(S,X) print_t3(S,X,"\n")
#define print_t_(X) print_t3(#X,X," ")
#define print_tn(X) print_t3(#X,X,"\n")


void time_stats()
{
	long sum;
	FILE *f = NULL;
	/*
	char name[100];
	sprintf(name, "%s/%s", USBLOADER_PATH, "debug.txt");
	f = fopen(name, "w");
	if (!f) {
		printf("Error opening %s\n", name);
	} else {
		fprintf(f,"CFG v%s\n", CFG_VERSION_STR);
	}
	*/
	printx("times in seconds:\n");
	tm_sum = 0;
	print_t_(intro);
	print_tn(wpad);
	print_t_(ios1);
	print_tn(ios2);
	print_t_(sd_init);
	print_tn(sd_mount);
	print_t_(usb_init);
	print_t3("mount", usb_mount, " ");
	print_t3("retry", usb_retry, "\n");
	printx("open: %.3f ini: %.3f cap: %.3f\n",
		diff_fsec(TIME.usb_init1, TIME.usb_open),
		diff_fsec(TIME.usb_open, TIME.usb_cap),
		diff_fsec(TIME.usb_cap, TIME.usb_init2));
	print_t3("cfg", cfg, " (config,settings,titles,theme)\n");
	print_t3("misc", misc, " (lang,playstat,unifont)\n");
	print_t_(wiitdb);
	printx("load: %.3f parse: %.3f\n", TIME_S(db_load), TIME_S(wiitdb)-TIME_S(db_load));
	print_t_(gamelist);
	print_tn(mp3);
	print_t_(conbg);
	print_tn(guitheme);
	sum = tm_sum;
	printx("sum: %.3f ", (float)sum/1000.0);
	printx("uncounted: %.3f\n", (float)((long)TIME_MS(boot)-sum)/1000.0);
	print_t2("total startup", boot);

	if (f) fclose(f);
}

void time_stats2()
{
	FILE *f = NULL;
	long sum;
	if (!CFG.time_launch) return;
	printx("times in seconds:\n");
	tm_sum = 0;
	print_tn(rios);
	print_tn(gcard);
	print_tn(playlog);
	print_tn(playstat);
	print_tn(load);
	sum = tm_sum;
	printx("sum: %.3f ", (float)sum/1000.0);
	printx("uncounted: %.3f\n", (float)((long)TIME_MS(run)-sum)/1000.0);
	print_t2("total launch", run);
	printx("size: %.2f MB\n", (float)TIME.size / 1024.0 / 1024.0);
	printx("speed: %.2f MB/s\n",
			(float)TIME.size / (float)TIME_MS(load) * 1000.0 / 1024.0 / 1024.0);
	extern void Menu_PrintWait();
	Menu_PrintWait();
}


