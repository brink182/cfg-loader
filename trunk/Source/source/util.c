#include <ogcsys.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

extern void* SYS_AllocArena2MemLo(u32 size,u32 align);

char* strcopy(char *dest, char *src, int size)
{
	strncpy(dest,src,size);
	dest[size-1] = 0;
	return dest;
}

bool str_replace(char *str, char *olds, char *news, int size)
{
	char *p;
	int len;
	p = strstr(str, olds);
	if (!p) return false;
	// new len
	len = strlen(str) - strlen(olds) + strlen(news);
	// check size
	if (len >= size) return false;
	// move remainder to fit (and nul)
	memmove(p+strlen(news), p+strlen(olds), strlen(p)-strlen(olds)+1);
	// copy new in place
	memcpy(p, news, strlen(news));
	// terminate
	str[len] = 0;
	return true;
}


extern long long gettime();
extern u32 diff_msec(long long start, long long end);

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


void* LARGE_memalign(size_t align, size_t size)
{
	return SYS_AllocArena2MemLo(size, align);
}

void LARGE_free(void *ptr)
{
	// nothing
}


