
// by oggzee

#ifndef _UTIL_H
#define _UTIL_H

#include <stdlib.h> // bool...
#include <gctypes.h> // bool...

#include "mem.h"
#include "debug.h"
#include "strutil.h"

#define D_S(A) A, sizeof(A)

void wiilight(int enable);

#include "memcheck.h"

#define MAX_USORT_MAP 1024
extern int usort_map[MAX_USORT_MAP];
extern int ufont_map[];
int map_ufont(int c);

int  mbs_len(char *s);
bool mbs_trunc(char *mbs, int n);
char*mbs_align(const char *str, int n);
int  mbs_coll(char *a, char *b);
int  mbs_len_valid(char *s);
char *mbs_copy(char *dest, char *src, int size);

int  con_char_len(int c);
int  con_len(char *s);
bool con_trunc(char *s, int n);
char*con_align(const char *str, int n);


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

void parse_banner_title(void *banner, u8 *title, s32 lang);
void parse_banner_snd(void *banner, SoundInfo *snd);

void printf_(const char *fmt, ...);
void printf_x(const char *fmt, ...);
void printf_h(const char *fmt, ...);

int mkpath(const char *s, int mode);

#endif

