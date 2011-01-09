
// by oggzee & usptactical

#ifndef _CACHE_H_
#define _CACHE_H_

#include <ogc/lwp.h>
#include <ogc/cond.h>
#include <ogc/mutex.h>

#include "my_GRRLIB.h"

#define CS_IDLE     0
#define CS_LOADING  1
#define CS_PRESENT  2
#define CS_MISSING  3

struct Cover_State 
{
	char used;
	char id[8];
	int gid;    // ccache.game[gid]
	GRRLIB_texImg tx;
	//int lru;
};

struct Game_State
{
	int request;
	int state; // idle, loading, present, missing
	int cid; // cache index
	int agi; // actual all_gameList[agi] (mapping gameList -> all_gameList)
	char id[8];
};

struct Cover_Cache
{
	void *data;
	int width4, height4;
	int csize;
	int num, cover_alloc, lru;
	struct Cover_State *cover;
	int num_game, game_alloc;
	struct Game_State *game;
	volatile int run;
	volatile int idle;
	volatile int restart;
	volatile int quit;
	lwp_t lwp;
	lwpq_t queue;
	mutex_t mutex;
};

extern struct Cover_Cache ccache;

extern int ccache_inv;

int cache_game_find(int gi);
void cache_request(int, int, int);
void cache_request_before_and_after(int, int, int);
void cache_release_all(void);
int  Cache_Init(void);
void Cache_Close(void);
void Cache_Invalidate(void);
void cache_wait_idle(void);

//#define DEBUG_CACHE
void draw_Cache();

#endif

