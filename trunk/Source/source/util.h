
// by oggzee

#ifndef _UTIL_H
#define _UTIL_H

#include <stdlib.h> // bool...
#include <gctypes.h> // bool...

#define STRCOPY(DEST,SRC) strcopy(DEST,SRC,sizeof(DEST)) 
char* strcopy(char *dest, const char *src, int size);

#define STRAPPEND(DST,SRC) strappend(DST,SRC,sizeof(DST))
char *strappend(char *dest, char *src, int size);

bool str_replace(char *str, char *olds, char *news, int size);
bool trimsplit(char *line, char *part1, char *part2, char delim, int size);

#define dbg_printf if (CFG.debug) printf
void dbg_time1();
unsigned dbg_time2(char *msg);

#define D_S(A) A, sizeof(A)

void util_init();
void util_clear();
void* LARGE_memalign(size_t align, size_t size);
void LARGE_free(void *ptr);
size_t LARGE_used();
void memstat2();

#define SAFE_FREE(P) if(P){free(P);P=NULL;}

void wiilight(int enable);

//#include "memcheck.h"

#ifndef _MEMCHECK_H
#define memstat() do{}while(0)
#define memcheck() do{}while(0)
#define memcheck_ptr(B,P) do{}while(0)
#endif

#define MAX_USORT_MAP 512
extern int usort_map[MAX_USORT_MAP];
extern int ufont_map[];
int map_ufont(int c);

int mbs_len(char *s);
bool mbs_trunc(char *mbs, int n);
int mbs_coll(char *a, char *b);

static inline u32 _be32(const u8 *p)
{
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static inline u32 _le32(const void *d)
{
	const u8 *p = d;
	return (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
}

static inline u32 _le16(const void *d)
{
	const u8 *p = d;
	return (p[1] << 8) | p[0];
}

void hex_dump1(void *p, int size);
void hex_dump2(void *p, int size);
void hex_dump3(void *p, int size);

typedef struct SoundInfo
{
	void *dsp_data;
	int size;
	int channels;
	int rate;
	int loop;
} SoundInfo;

void parse_banner_snd(void *data_hdr, SoundInfo *snd);

#endif

