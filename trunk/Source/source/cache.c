
// by oggzee & usptactical

#include <stdio.h>
#include <stdlib.h>
#include <ogcsys.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>

#include "cache.h"
#include "disc.h"
#include "gui.h"
#include "cfg.h"

#define COVER_CACHE_DATA_SIZE 0x2000000

extern struct discHdr *all_gameList;
extern struct discHdr *gameList;
extern s32 gameCnt, all_gameCnt;

struct Cover_Cache ccache;
int ccache_init = 0;
int ccache_inv = 0;

//****************************************************************************
//**************************  START CACHE CODE  ******************************
//****************************************************************************

void game_populate_ids()
{
	int i;
	char *id;

	for (i=0; i<all_gameCnt; i++) {
		memcheck_ptr(all_gameList, &all_gameList[i]);
		id = (char*)all_gameList[i].id;
		memcheck_ptr(ccache.game,&ccache.game[i]);
		STRCOPY(ccache.game[i].id, id);
	}
}

int cache_game_find_id(int gi)
{
	int i;
	bool getNewIds = true;
	char *id = (char*)gameList[gi].id;
	
	retry:;
	for (i=0; i<all_gameCnt; i++) {
		memcheck_ptr(ccache.game,&ccache.game[i]);
		if (strcmp(ccache.game[i].id, id) == 0) {
			ccache.game[gi].agi = i;
			return i;
		}
	}
	//didn't find the game so lets load up the 
	//ids in case they're not loaded yet
	if (getNewIds) {
		game_populate_ids();
		getNewIds = false;
		goto retry;
	}
	//should never get here...
	return -1;
}

// find gameList[gi] in all_gameList[]
int cache_game_find(int gi)
{
	// try if direct mapping exists
	int agi = ccache.game[gi].agi;
	if ( (strcmp(ccache.game[agi].id, (char*)gameList[gi].id) == 0) &&
	     (strcmp(ccache.game[agi].id, (char*)all_gameList[agi].id) == 0) )
	{
		return agi;
	}
	// direct mapping not found, search for id
	return cache_game_find_id(gi);
}

int cache_find(char *id)
{
	int i;
	for (i=0; i<ccache.num; i++) {
		if (strcmp(ccache.cover[i].id, id) == 0) {
			return i;
		}
	}
	return -1;
}

int cache_find_free()
{
	int i;
	for (i=0; i<ccache.num; i++) {
		if (*ccache.cover[i].id == 0) {
			return i;
		}
	}
	return -1;
}

int cache_find_lru()
{
	int i, idx;
	//int lru = -1;
	//int found = -1;
	// not a true lru, but good enough
	for (i=0; i<ccache.num; i++) {
		idx = (ccache.lru + i) % ccache.num;
		if (ccache.cover[idx].used == 0) {
			ccache.lru = (idx + 1) % ccache.num;
			return idx;
		}
	}
	return -1;
}

void cache_free(int i)
{
	memcheck_ptr(ccache.cover, &ccache.cover[i]);
	int gid = ccache.cover[i].gid;
	if (*ccache.cover[i].id && gid >= 0) {
		memcheck_ptr(ccache.game, &ccache.game[gid]);
		ccache.game[gid].state = CS_IDLE;
	}
	*ccache.cover[i].id = 0;
	ccache.cover[i].used = 0;
	//ccache.cover[i].lru = 0;
	ccache.cover[i].gid = -1;
}

int cache_alloc(char *id)
{
	int i;
	i = cache_find(id);
	if (i >= 0) goto found;
	i = cache_find_free();
	if (i >= 0) goto found;
	i = cache_find_lru();
	if (i >= 0) goto found;
	return -1;
	found:
	cache_free(i);
	return i;
}

#ifdef DEBUG_CACHE

struct Cache_Debug
{
	int cc_rq_prio;
	int cc_i;
	int cc_line;
	int cc_line_lock;
	int cc_line_unlock;
	int cc_locked;
	int cc_cid;
	void *cc_img;
	void *cc_buf;
};

struct Cache_Debug ccdbg;

#define ___ ccdbg.cc_line = __LINE__
#define CACHE_LOCK() do{ \
	ccdbg.cc_line_lock = __LINE__; \
	LWP_MutexLock(ccache.mutex); \
	ccdbg.cc_locked = 1; }while(0)

#define CACHE_UNLOCK() do{ \
	ccdbg.cc_line_unlock = __LINE__; \
	LWP_MutexUnlock(ccache.mutex); \
	ccdbg.cc_locked = 0; }while(0)

#define CCDBG(X) X

#else

#define ___
#define CACHE_LOCK() LWP_MutexLock(ccache.mutex)
#define CACHE_UNLOCK() LWP_MutexUnlock(ccache.mutex)
#define CCDBG(X) 

#endif


/**
 * This is the main cache thread. This method loops through all the game indexes  
 * (0 through ccache.num_game) and loads each requested cover. The covers are loaded
 * based on the priority level (rq_prio) -> 1 is highest priority, 3 is lowest.
 *
 */

void* cache_thread(void *arg)
{
	int i, ret, actual_i;
	char *id;
	void *img;
	void *buf;
	int cid;
	int rq_prio;
	bool resizeToFullCover = false;
	int cover_height, cover_width;
	//dbg_printf("thread started\n");

	___;
	while (!ccache.quit) {
		___;
		CACHE_LOCK();
		ccache.idle = 0;
		//dbg_printf("thread running\n");

		restart:

		ccache.restart = 0;
		___;
		//if (0) // disable
		for (rq_prio=1; rq_prio<=3; rq_prio++) {
			___;
			for (i=0; i<ccache.num_game; i++) {
				___;
				CCDBG(ccdbg.cc_rq_prio = rq_prio);
				CCDBG(ccdbg.cc_i = i);
				if (ccache.restart) goto restart;
				//get the actual game index, in case we're using favorites
				actual_i = cache_game_find(i);
				memcheck_ptr(ccache.game, &ccache.game[actual_i]);
				if (ccache.game[actual_i].request != rq_prio) continue;  //was "<"
				// load
				ccache.game[actual_i].state = CS_LOADING;
				memcheck_ptr(gameList, &gameList[i]);
				id = (char*)gameList[i].id;
				
				//dbg_printf("thread processing %d %s\n", i, id);
				img = NULL;
				___;
				CACHE_UNLOCK();
				___;

				//capture the current cover width and height
				cover_height = COVER_HEIGHT;
				cover_width = COVER_WIDTH;

				//load the cover image
				if (CFG.cover_style == CFG_COVER_STYLE_FULL) {
					//try to load the full cover
					ret = Gui_LoadCover_style((u8*)id, &img, false, CFG_COVER_STYLE_FULL);
					if (ret < 0) {
						//try to load the 2D cover
						ret = Gui_LoadCover_style((u8*)id, &img, true, CFG_COVER_STYLE_2D);
						if (ret && img) resizeToFullCover = true;
					}
				} else {
					ret = Gui_LoadCover((u8*)id, &img, false);
				}
				//sleep(1);//dbg
				___;
				if (ccache.quit) goto quit;
				___;
				CACHE_LOCK();
				___;
				if (ret > 0 && img) {
					___;
					cid = cache_alloc(id);
					if (cid < 0) {
						// should not happen
						free(img);
						ccache.game[actual_i].state = CS_IDLE;
						goto end_request;
					}
					buf = ccache.data + ccache.csize * cid;
					___;
					CACHE_UNLOCK();
					___;
					memcheck_ptr(ccache.cover, &ccache.cover[cid]);
					CCDBG(ccdbg.cc_img = img);
					CCDBG(ccdbg.cc_buf = buf);
					CCDBG(ccdbg.cc_cid = cid);
					//sleep(3);//dbg

					LWP_MutexUnlock(ccache.mutex);

					//make sure the cover height and width didn't change on us before we resize it
					if ((cover_height == COVER_HEIGHT) &&
						(cover_width == COVER_WIDTH)) {

						if (resizeToFullCover) {
							ccache.cover[cid].tx = Gui_LoadTexture_fullcover(img, COVER_WIDTH, COVER_HEIGHT, COVER_WIDTH_FRONT, buf);
							resizeToFullCover = false;
						} else {
							if (CFG.cover_style == CFG_COVER_STYLE_FULL && CFG.gui_compress_covers)
								ccache.cover[cid].tx = Gui_LoadTexture_CMPR(img, COVER_WIDTH, COVER_HEIGHT, buf);
							else
								ccache.cover[cid].tx = Gui_LoadTexture_RGBA8(img, COVER_WIDTH, COVER_HEIGHT, buf);
						}
					} else {
						CCDBG(ccdbg.cc_img = NULL);
						CCDBG(ccdbg.cc_buf = NULL);
						free(img);
						if (ccache.quit) goto quit;
						CACHE_LOCK();
						goto end_request;
					}
					
					CCDBG(ccdbg.cc_img = NULL);
					CCDBG(ccdbg.cc_buf = NULL);
					free(img);
					___;
					if (ccache.quit) goto quit;
					___;
					CACHE_LOCK();
					___;
					if (ccache.cover[cid].tx.data == NULL) {
						cache_free(cid);
						goto noimg;
					}
					// mark
					STRCOPY(ccache.cover[cid].id, id);
					// check if req change
					if (ccache.game[actual_i].request) {
						ccache.cover[cid].used = 1;
						ccache.cover[cid].gid = actual_i;
						ccache.game[actual_i].cid = cid;
						ccache.game[actual_i].state = CS_PRESENT;
						//dbg_printf("Load OK %d %d %s\n", i, cid, id);

					} else {
						ccache.game[actual_i].state = CS_IDLE;
					}
				} else {
					noimg:
					ccache.game[actual_i].state = CS_MISSING;
					//dbg_printf("Load FAIL %d %s\n", i, id);
				}
				end_request:
				// request processed.
				ccache.game[actual_i].request = 0;
				___;
			}
		}
		___;
		CCDBG(ccdbg.cc_rq_prio = 0);
		CCDBG(ccdbg.cc_i = -1);
		ccache.idle = 1;
		//dbg_printf("thread idle\n");
		// all processed. wait.
		while (!ccache.restart && !ccache.quit) {
			___;
			CACHE_UNLOCK();
			//dbg_printf("thread sleep\n");
			___;
			LWP_ThreadSleep(ccache.queue);
			//dbg_printf("thread wakeup\n");
			___;
			CACHE_LOCK();
			___;
		}
		___;
		ccache.restart = 0;
		CACHE_UNLOCK();
	}

	quit:
	___;
	ccache.quit = 0;

	return NULL;
}

#undef ___

void cache_wait_idle()
{
	int i;
	if (!ccache_init) return;
	// wait till idle
	i = 1000; // 1 second
	while (!ccache.idle && i) {
		usleep(1000);
		i--;
	}
}

/**
 * This method is used to set the caching priority of the passed in game index.
 * If the image is already cached or missing then the CoverCache is updated for
 * the cover.
 *
 * @param game_i the game index
 * @param rq_prio the cache priority: 1 = high priority
 */
void cache_request_1(int game_i, int rq_prio)
{
	int cid, actual_i;
	if (game_i < 0 || game_i >= ccache.num_game) return;

	actual_i = cache_game_find(game_i);
	if (ccache.game[actual_i].request>0) return;
	// check if already present
	if (ccache.game[actual_i].state == CS_PRESENT) {
		cid = ccache.game[actual_i].cid;
		ccache.cover[cid].used = 1;
		return;
	}
	// check if already missing
	if (ccache.game[actual_i].state == CS_MISSING) {
		return;
	}
	// check if already cached
	cid = cache_find((char*)gameList[game_i].id);
	if (cid >= 0) {
		ccache.cover[cid].used = 1;
		ccache.cover[cid].gid = actual_i;
		ccache.game[actual_i].cid = cid;
		ccache.game[actual_i].state = CS_PRESENT;
		return;
	}
	// add request
	ccache.game[actual_i].request = rq_prio;
}


/**
 * This method is used to set the caching priority of the passed in game index
 * as well as the next x number of games.
 *
 * @param game_i the starting game index
 * @param num the number of images to set the priority on
 * @param rq_prio the cache priority: 1 = high priority
 */
void cache_request(int game_i, int num, int rq_prio)
{
	int i, idle;
	// setup requests
	CACHE_LOCK();
	for (i=0; i<num; i++) {
		cache_request_1(game_i + i, rq_prio);
	}
	// restart thread
	ccache.restart = 1;
	idle = ccache.idle;
	CACHE_UNLOCK();
	//dbg_printf("cache restart\n");
	LWP_ThreadSignal(ccache.queue);
	if (idle) {
		//dbg_printf("thread idle wait restart\n");
		i = 10;
		while (ccache.restart && i) {
			usleep(10);
			LWP_ThreadSignal(ccache.queue);
			i--;
		}
		if (ccache.restart) {
			//dbg_printf("thread fail restart\n");
		}
	}
	//dbg_printf("cache restart done\n");
}


/**
 * This method is used to set the caching priority of the passed in game index
 * as well as the next x number of games before and after the index.  All the
 * rest of the covers are given a lower priority.
 *
 * @param game_i the starting game index
 * @param num the number of images before and after the game_i to set the priority on
 * @param rq_prio the cache priority: 1 = high priority
 */
void cache_request_before_and_after(int game_i, int num, int rq_prio)
{
	int i, idle;
	int nextRight, nextLeft;

	// setup requests
	CACHE_LOCK();

	// set the first one high
	cache_request_1(game_i, rq_prio);
	
	// alternate the rest
	nextRight = game_i + 1;
	nextLeft = game_i - 1;
	for (i=0; i<num; i++) {
		if (nextRight >= ccache.num_game)
			nextRight = 0;
		cache_request_1(nextRight, 2);
		
		if (nextLeft < 0)
			nextLeft = ccache.num_game - 1;
		cache_request_1(nextLeft, 2);
		
		nextRight++;
		nextLeft--;
	}
	
	// now set the priority to low for +-10 covers
	for (i=0; i<10; i++) {
		if (nextRight >= ccache.num_game)
			nextRight = 0;
		cache_request_1(nextRight, 3);
		
		if (nextLeft < 0)
			nextLeft = ccache.num_game - 1;
		cache_request_1(nextLeft, 3);
		
		nextRight++;
		nextLeft--;
	}
	
	// restart thread
	ccache.restart = 1;
	idle = ccache.idle;
	CACHE_UNLOCK();
	LWP_ThreadSignal(ccache.queue);
	if (idle) {
		//dbg_printf("thread idle wait restart\n");
		i = 10;
		while (ccache.restart && i) {
			usleep(10);
			LWP_ThreadSignal(ccache.queue);
			i--;
		}
		if (ccache.restart) {
			//dbg_printf("thread fail restart\n");
		}
	}
	//dbg_printf("cache restart done\n");
}


void cache_release_1(int game_i)
{
	int cid;
	if (game_i < 0 || game_i >= ccache.num_game) return;
	memcheck_ptr(ccache.game, &ccache.game[game_i]);
	ccache.game[game_i].request = 0;
	if (ccache.game[game_i].state == CS_PRESENT) {
		// release if already cached
		cid = ccache.game[game_i].cid;
		memcheck_ptr(ccache.cover, &ccache.cover[cid]);
		ccache.cover[cid].used = 0; //--
		ccache.game[game_i].state = CS_IDLE;
	}
}

void cache_release(int game_i, int num)
{
	int i;
	// cancel requests
	CACHE_LOCK();
	for (i=0; i<num; i++) {
		cache_release_1(game_i + i);
	}
	CACHE_UNLOCK();
}

void cache_release_all()
{
	int i;
	// cancel all requests
	CACHE_LOCK();
	for (i=0; i<ccache.num_game; i++) {
		cache_release_1(i);
	}
	CACHE_UNLOCK();
}

/**
 * Reallocates cache tables, if gamelist resized
 */
int cache_resize_tables()
{
	bool resized = false;
	int size, new_num;

	//dbg_printf("\ncache_resize %p %d %d %p %d %d\n",
	//		ccache.game, ccache.game_alloc, ccache.num_game,
	//		ccache.cover, ccache.cover_alloc, ccache.num);


	memcheck();
	// game table
	// resize if games added
	if (all_gameCnt > ccache.game_alloc || ccache.game == NULL)
	{
		ccache.game_alloc = all_gameCnt;
		size = sizeof(struct Game_State) * ccache.game_alloc;
		ccache.game = realloc(ccache.game, size);
		if (ccache.game == NULL) goto err;
		resized = true;
	}
	ccache.num_game = gameCnt;

	memcheck();
	// cover table
	// check cover size

	if (CFG.cover_style == CFG_COVER_STYLE_FULL && CFG.gui_compress_covers) {
		ccache.width4 = COVER_WIDTH;
		ccache.height4 = COVER_HEIGHT;
		ccache.csize = (ccache.width4 * ccache.height4)/2;
	} else {
		ccache.width4 = (COVER_WIDTH + 3) >> 2 << 2;
		ccache.height4 = (COVER_HEIGHT + 3) >> 2 << 2;
		ccache.csize = ccache.width4 * ccache.height4 * 4;
	}
	
	new_num = COVER_CACHE_DATA_SIZE / ccache.csize;
	if (new_num > ccache.cover_alloc || ccache.cover == NULL) {
		ccache.cover_alloc = new_num;
		size = sizeof(struct Cover_State) * ccache.cover_alloc;
		ccache.cover = realloc(ccache.cover, size);
		if (ccache.cover == NULL) goto err;
		resized = true;
	}
	ccache.num = new_num;

	memcheck();
	if (resized) {
		// clear both tables
		size = sizeof(struct Game_State) * ccache.game_alloc;
		memset(ccache.game, 0, size);
		size = sizeof(struct Cover_State) * ccache.cover_alloc;
		memset(ccache.cover, 0, size);
		ccache.lru = 0;
	}
	memcheck();

	//dbg_printf("cache_resize %p %d %d %p %d %d\n",
	//		ccache.game, ccache.game_alloc, ccache.num_game,
	//		ccache.cover, ccache.cover_alloc, ccache.num);

	return 0;

err:
	// fatal
	printf("ERROR: cache: out of memory\n");
	sleep(1);
	return -1;
}	

/**
 * Initializes the cover cache
 */
int Cache_Init()
{
	int ret;

	//update the cache tables size
	if (ccache_inv || !ccache_init) {
		// call invalidate again, for safety
		// this will cancel all requests and wait till idle
		Cache_Invalidate();
		ret = cache_resize_tables();
		if (ret) return -1;
		ccache_inv = 0;
	}
	ccache.num_game = gameCnt;

	if (ccache_init) return 0;
	ccache_init = 1;
	
	ccache.data = LARGE_memalign(32, COVER_CACHE_DATA_SIZE);

	ccache.lwp   = LWP_THREAD_NULL;
	ccache.queue = LWP_TQUEUE_NULL;
	ccache.mutex = LWP_MUTEX_NULL;

	// start thread
	LWP_InitQueue(&ccache.queue);
	LWP_MutexInit(&ccache.mutex, FALSE);
	ccache.run = 1;
	//dbg_printf("running thread\n");
	ret = LWP_CreateThread(&ccache.lwp, cache_thread, NULL, NULL, 32*1024, 40);
	if (ret < 0) {
		//cache_thread_stop();
		return -1;
	}
	//dbg_printf("lwp ret: %d id: %x\n", ret, ccache.lwp);
	return 0;
}

void Cache_Invalidate()
{
	if (!ccache_init) return;
	CACHE_LOCK();
	// clear requests
	memset(ccache.game, 0, sizeof(struct Game_State) * ccache.game_alloc);
	CACHE_UNLOCK();
	// wait till idle
	cache_wait_idle();
	if (!ccache.idle) {
		//printf("\nERROR: cache not idle\n"); sleep(3);
	}
	// clear all
	memset(ccache.game, 0, sizeof(struct Game_State) * ccache.game_alloc);
	memset(ccache.cover, 0, sizeof(struct Cover_State) * ccache.cover_alloc);

	ccache_inv = 1;
}

void Cache_Close()
{
	int i;
	if (!ccache_init) return;
	ccache_init = 0;
	CACHE_LOCK();
	ccache.quit = 1;
	CACHE_UNLOCK();
	LWP_ThreadSignal(ccache.queue);
	i = 1000; // 1 second
	while (ccache.quit && i) {
		usleep(1000);
		LWP_ThreadSignal(ccache.queue);
		i--;
	}
	if (ccache.quit) {
		printf("\nERROR: Cache Close\n");
		sleep(5);
	} else {
		LWP_JoinThread(ccache.lwp, NULL);
	}
	LWP_CloseQueue(ccache.queue);
	LWP_MutexDestroy(ccache.mutex);
	ccache.lwp   = LWP_THREAD_NULL;
	ccache.queue = LWP_TQUEUE_NULL;
	ccache.mutex = LWP_MUTEX_NULL;
	SAFE_FREE(ccache.cover);
	SAFE_FREE(ccache.game);
}

void draw_Cache()
{
#ifdef DEBUG_CACHE
	unsigned int i, x, y, c, cid;

	// thread state
	x = 15;
	y = 30;
	c = ccache.idle ? 0x00FF00FF : 0xFF0000FF;
	GRRLIB_Rectangle(x, y, 8, 8, c, 1);
	x += 10;
	c = !ccache.restart ? 0x00FF00FF : 0xFF0000FF;
	GRRLIB_Rectangle(x, y, 8, 8, c, 1);
	x += 10;
	c = !ccache.quit ? 0x00FF00FF : 0xFF0000FF;
	GRRLIB_Rectangle(x, y, 8, 8, c, 1);
	x += 20;
	GRRLIB_Printf(x, y, tx_cfont, 0xFFFFFFFF, 1, "%d %d %d",
			ccdbg.cc_rq_prio, ccdbg.cc_i, ccdbg.cc_line);
	// lock state
	x = 15;
	y = 40;
	c = !ccdbg.cc_locked ? 0x00FF00FF : 0xFF0000FF;
	GRRLIB_Rectangle(x, y, 8, 8, c, 1);
	x += 40;
	GRRLIB_Printf(x, y, tx_cfont, 0xFFFFFFFF, 1, "%d %d %p %p %d",
			ccdbg.cc_line_lock, ccdbg.cc_line_unlock,
			ccdbg.cc_img, ccdbg.cc_buf, ccdbg.cc_cid);

	// cover state
	for (i=0; i<ccache.num; i++) {
		x = 15 + (i % 10) * 2;
		y = 50 + (i / 10) * 2;
		// free: black
		// used: blue
		// present: green
		// add lru to red?
		if (ccache.cover[i].id[0] == 0) {
			c = 0x000000FF;
		} else if (ccache.cover[i].used) {
			c = 0x8080FFFF;
		} else {
			c = 0x80FF80FF;
		}
		GRRLIB_Rectangle(x, y, 2, 2, c, 1);
	}

	// game state
	for (i=0; i<ccache.num_game; i++) {
		x = 15 + (i % 10)*2;
		y = 110 + (i / 10)*2;
		// idle: black
		// loading: yellow
		// used: blue
		// present: green
		// missing: red
		// request: violet
		if (ccache.game[i].request) {
			c = 0xFF00FFFF;
		} else
		switch (ccache.game[i].state) {
			case CS_LOADING: c = 0xFFFF80FF; break;
			case CS_PRESENT:
				cid = ccache.game[i].cid;
				if (ccache.cover[cid].used) {
					c = 0x8080FFFF;
				} else {
					c = 0x80FF80FF;
				}
				break;
			case CS_MISSING: c = 0xFF8080FF; break;
			default:
			case CS_IDLE: c = 0x000000FF; break;
		}
		//GRRLIB_Plot(x, y, c);
		GRRLIB_Rectangle(x, y, 2, 2, c, 1);
	}
#endif
}


//****************************************************************************
//****************************  END CACHE CODE  ******************************
//****************************************************************************
