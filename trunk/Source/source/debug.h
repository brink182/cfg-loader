
// by oggzee

#ifndef _DEBUG_H
#define _DEBUG_H

void InitDebug();
//#define dbg_printf if (CFG.debug) printf
int dbg_printf(const char *fmt, ... );

void dbg_time1();
unsigned dbg_time2(char *msg);


#endif

