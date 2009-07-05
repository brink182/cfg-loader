#include <stdio.h>
#include <stdlib.h>
#include <ogcsys.h>
#include <malloc.h>
#include <string.h>

#include "fat.h"
#include "video.h"
#include "cfg.h"
#include "wpad.h"

#include "libpng/png.h"
#include "GRRLIB.h"

/* Constants */
/*
#define CONSOLE_XCOORD		258
#define CONSOLE_YCOORD		112
#define CONSOLE_WIDTH		354
#define CONSOLE_HEIGHT		304

#define COVER_XCOORD		24
#define COVER_YCOORD		104

#define COVER_WIDTH			160
#define COVER_HEIGHT		225
*/

int COVER_WIDTH = 160;
int COVER_HEIGHT = 225;

#define BACKGROUND_WIDTH	640
#define BACKGROUND_HEIGHT	480

// #define USBLOADER_PATH		"sd:/usb-loader"

extern bool imageNotFound;

#define SAFE_PNGU_RELEASE(CTX) if(CTX){PNGU_ReleaseImageContext(CTX);CTX=NULL;}

s32 __Gui_DrawPngA(void *img, u32 x, u32 y);
s32 __Gui_DrawPngRA(void *img, u32 x, u32 y, int width, int height);
void CompositeRGBA(char *bg_buf, int bg_w, int bg_h,
		int x, int y, char *img, int width, int height);
void ResizeRGBA(char *img, int imgWidth, int imgHeight,
	   char *resize, int width, int height);

extern unsigned char bgImg[];
extern unsigned char bgImg_wide[];
extern unsigned char bg_gui[];
extern unsigned char coverImg[];
extern unsigned char coverImg_wide[];
extern unsigned char pointer[];
extern unsigned char hourglass[];
extern unsigned char star_icon[];
extern unsigned char console_font[];


s32 __Gui_GetPngDimensions(void *img, u32 *w, u32 *h)
{
	IMGCTX   ctx = NULL;
	PNGUPROP imgProp;

	s32 ret;

	/* Select PNG data */
	ctx = PNGU_SelectImageFromBuffer(img);
	if (!ctx) {
		ret = -1;
		goto out;
	}

	/* Get image properties */
	ret = PNGU_GetImageProperties(ctx, &imgProp);
	if (ret != PNGU_OK) {
		ret = -1;
		goto out;
	}

	/* Set image width and height */
	*w = imgProp.imgWidth;
	*h = imgProp.imgHeight;

	/* Success */
	ret = 0;

out:
	/* Free memory */
	if (ctx)
		PNGU_ReleaseImageContext(ctx);

	return ret;
}

s32 __Gui_DrawPng(void *img, u32 x, u32 y)
{
	IMGCTX   ctx = NULL;
	PNGUPROP imgProp;

	s32 ret;

	/* Select PNG data */
	ctx = PNGU_SelectImageFromBuffer(img);
	if (!ctx) {
		ret = -1;
		goto out;
	}

	/* Get image properties */
	ret = PNGU_GetImageProperties(ctx, &imgProp);
	if (ret != PNGU_OK) {
		ret = -1;
		goto out;
	}

	/* Draw image */
	Video_DrawPng(ctx, imgProp, x, y);

	/* Success */
	ret = 0;

out:
	/* Free memory */
	if (ctx)
		PNGU_ReleaseImageContext(ctx);

	return ret;
}


void Gui_InitConsole(void)
{
	/* Initialize console */
	Con_Init(CONSOLE_XCOORD, CONSOLE_YCOORD, CONSOLE_WIDTH, CONSOLE_HEIGHT);
}

void Gui_DrawBackground(void)
{
	void *builtin = (void *)bgImg;
	void *imgData;

	s32 ret = 0;

	if (CFG.widescreen) {
		builtin = bgImg_wide;
	}
	imgData = builtin;

	/* Try to open background image from SD */
	if (*CFG.background)
		ret = Fat_ReadFile(CFG.background, &imgData);
	if (ret > 0) {
		u32 width, height;

		/* Get image dimensions */
		__Gui_GetPngDimensions(imgData, &width, &height);

		/* Background is too big, use default background */
		if ((width > BACKGROUND_WIDTH) || (height > BACKGROUND_HEIGHT))
			imgData = (void *)builtin;
	}

	// save background
	Video_SaveBg(imgData);

	/* Draw background */
	//__Gui_DrawPng(imgData, 0, 0);
	Video_DrawBg();

	/* Free memory */
	if (imgData != builtin)
		free(imgData);
} 

int Gui_LoadCover(u8 *discid, void **p_imgData, bool load_noimage)
{
	int ret = -1;
	char filepath[200];
	char *wide = "";

	if (*discid) {
		if (CFG.widescreen) wide = "_wide";
		retry_open:
		/* Generate cover filepath */
		snprintf(filepath, sizeof(filepath), "%s/%.6s%s.png",
				CFG.covers_path, discid, wide);

		/* Open cover */
		ret = Fat_ReadFile(filepath, p_imgData);
		if (ret < 0) {
			// if 6 character id not found, try 4 character id
			snprintf(filepath, sizeof(filepath), "%s/%.4s%s.png",
					CFG.covers_path, discid, wide);
			ret = Fat_ReadFile(filepath, p_imgData);
		}
		if (ret < 0 && *wide) {
			// retry with non-wide
			wide = "";
			goto retry_open;
		}
		if (ret < 0) {
			imageNotFound = true;
		}
	}
	if (ret < 0 && load_noimage) {
		if (CFG.widescreen) wide = "_wide";
		retry_open2:
		snprintf(filepath, sizeof(filepath), "%s/noimage%s.png",
				CFG.covers_path, wide);
		ret = Fat_ReadFile(filepath, p_imgData);
		if (ret < 0 && *wide) {
			// retry with non-wide noimage
			wide = "";
			goto retry_open2;
		}
	}
	return ret;
}

void DrawCoverImg(u8 *discid, char *img_buf, int width, int height)
{
	char *bg_buf = NULL;
	char *star_buf = NULL;
	IMGCTX ctx = NULL;
	s32 ret;

	// get background
	bg_buf = memalign(32, width * height * 4);
	if (!bg_buf) return;
	Video_GetBG(COVER_XCOORD, COVER_YCOORD, bg_buf, width, height);
	// composite img to bg
	CompositeRGBA(bg_buf, width, height, 0, 0,
				img_buf, width, height);
	// favorite?
	if (is_favorite(discid)) {
		// star
		PNGUPROP imgProp;
		// Select PNG data
		ctx = PNGU_SelectImageFromBuffer(star_icon);
		if (!ctx) goto out;
		// Get image properties
		ret = PNGU_GetImageProperties(ctx, &imgProp);
		if (ret != PNGU_OK) goto out;
		// alloc star img buf
		star_buf = memalign(32, imgProp.imgWidth * imgProp.imgHeight * 4);
		if (!star_buf) goto out;
		// decode
		ret = PNGU_DECODE_TO_COORDS_RGBA8(ctx, 0, 0,
				imgProp.imgWidth, imgProp.imgHeight, 255,
				imgProp.imgWidth, imgProp.imgHeight, star_buf);
		// composite star
		CompositeRGBA(bg_buf, width, height,
			   	COVER_WIDTH - imgProp.imgWidth, 0, // right upper corner
				star_buf, imgProp.imgWidth, imgProp.imgHeight);
		SAFE_FREE(star_buf);
		SAFE_PNGU_RELEASE(ctx);
	}
out:
	// draw
	Video_DrawRGBA(COVER_XCOORD, COVER_YCOORD, bg_buf, width, height);
	SAFE_FREE(bg_buf);
}

void Gui_DrawCover(u8 *discid)
{
	void *builtin;
	void *imgData;

	s32  ret;

	imageNotFound = false;

	if (CFG.covers == 0) return;

	if (CFG.widescreen) {
		builtin = coverImg_wide;
	} else {
		builtin = coverImg;
	}
	imgData = builtin;

	ret = Gui_LoadCover(discid, &imgData, true);

	if (ret < 0) imgData = builtin;

	u32 width, height;

	/* Get image dimensions */
	ret = __Gui_GetPngDimensions(imgData, &width, &height);

	/* If invalid image, use default noimage */
	if (ret) {
		imageNotFound = true;
		if (imgData != builtin) free(imgData);
		imgData = builtin;
		ret = __Gui_GetPngDimensions(imgData, &width, &height);
		if (ret) return;
	}

	char *img_buf = NULL;
	char *resize_buf = NULL;
	IMGCTX ctx = NULL;

	img_buf = memalign(32, COVER_WIDTH * COVER_HEIGHT * 4);
	if (!img_buf) { ret = -1; goto out; }

	// Select PNG data
	ctx = PNGU_SelectImageFromBuffer(imgData);
	if (!ctx) { ret = -1; goto out; }

	// resize needed?
	if ((width != COVER_WIDTH) || (height != COVER_HEIGHT)) {
		// alloc tmp resize buf
		resize_buf = memalign(32, width * height * 4);
		if (!resize_buf) { ret = -1; goto out; }
		// decode
		ret = PNGU_DECODE_TO_COORDS_RGBA8(ctx, 0, 0,
				width, height, 255, width, height, resize_buf);
		// resize
		ResizeRGBA(resize_buf, width, height, img_buf, COVER_WIDTH, COVER_HEIGHT);
		// discard tmp resize buf
		SAFE_FREE(resize_buf);
	} else {
		// decode
		ret = PNGU_DECODE_TO_COORDS_RGBA8(ctx, 0, 0,
				width, height, 255, width, height, img_buf);
	}

	SAFE_PNGU_RELEASE(ctx);

	DrawCoverImg(discid, img_buf, COVER_WIDTH, COVER_HEIGHT);

out:

	SAFE_FREE(img_buf);
	SAFE_FREE(resize_buf);
	SAFE_PNGU_RELEASE(ctx);

	/* Free memory */
	if (imgData != builtin)
		free(imgData);
}

s32 __Gui_DrawPngA(void *img, u32 x, u32 y)
{
	IMGCTX   ctx = NULL;
	PNGUPROP imgProp;

	s32 ret;

	/* Select PNG data */
	ctx = PNGU_SelectImageFromBuffer(img);
	if (!ctx) {
		ret = -1;
		goto out;
	}

	/* Get image properties */
	ret = PNGU_GetImageProperties(ctx, &imgProp);
	if (ret != PNGU_OK) {
		ret = -1;
		goto out;
	}

	char *img_buf;
	img_buf = memalign(32, imgProp.imgWidth * imgProp.imgHeight * 4);
	if (!img_buf) {
		ret = -1;
		goto out;
	}

	// decode
	ret = PNGU_DECODE_TO_COORDS_RGBA8(ctx, 0, 0, imgProp.imgWidth, imgProp.imgHeight, 255,
			imgProp.imgWidth, imgProp.imgHeight, img_buf);
	// combine
	Video_CompositeRGBA(x, y, img_buf, imgProp.imgWidth, imgProp.imgHeight);
	// draw
	Video_DrawRGBA(x, y, img_buf, imgProp.imgWidth, imgProp.imgHeight);

	free(img_buf);

	/* Success */
	ret = 0;

out:
	/* Free memory */
	if (ctx)
		PNGU_ReleaseImageContext(ctx);

	return ret;
}

void PadRGBA(char *img, int imgWidth, int imgHeight,
	   char *resize, int width, int height)
{
	int x, y;
	for (y=0; y<height; y++) {
		for (x=0; x<width; x++) {
			if (x < imgWidth && y < imgHeight) {
				((int*)resize)[y*width + x] = ((int*)img)[y*imgWidth + x];
			} else {
				((int*)resize)[y*width + x] = 0x00000000;
			}
		}
	}
}

void ResizeRGBA1(char *img, int imgWidth, int imgHeight,
	   char *resize, int width, int height)
{
	int x, y, ix, iy;
	for (y=0; y<height; y++) {
		for (x=0; x<width; x++) {
			ix = x * imgWidth / width;
			iy = y * imgHeight / height;
			((int*)resize)[y*width + x] =
				((int*)img)[iy*imgWidth + ix];
		}
	}
}

// resize img buf to resize buf
void ResizeRGBA(char *img, int imgWidth, int imgHeight,
	   char *resize, int width, int height)
{
	u32 x, y, ix, iy, i;
	u32 rx, ry, nx, ny;
	u32 fix, fnx, fiy, fny;

	//dbg_printf("Resize: (%d, %d) -> (%d, %d)\n", imgWidth, imgHeight, width, height);

	for (y=0; y<height; y++) {
		for (x=0; x<width; x++) {
			rx = (x << 8) * imgWidth / width;
			fnx = rx & 0xff;
			fix = 0x100 - fnx;
			ix = rx >> 8;
			nx = ix+1;
			if (nx >= imgWidth) nx = imgWidth - 1;

			ry = (y << 8) * imgHeight / height;
			fny = ry & 0xff;
			fiy = 0x100 - fny;
			iy = ry >> 8;
			ny = iy+1;
			if (ny >= imgHeight) ny = imgHeight - 1;

			u8 *rp  = (u8*)&((int*)resize)[y*width + x];
			u8 *i00 = (u8*)&((int*)img)[iy*imgWidth + ix];
			u8 *i01 = (u8*)&((int*)img)[iy*imgWidth + nx];
			u8 *i10 = (u8*)&((int*)img)[ny*imgWidth + ix];
			u8 *i11 = (u8*)&((int*)img)[ny*imgWidth + nx];
			for (i=0; i<4; i++) {
				rp[i] =
					(
					( ( (u32)i00[i] * fix + (u32)i01[i] * fnx ) * fiy )
					+
					( ( (u32)i10[i] * fix + (u32)i11[i] * fnx ) * fny )
					) >> 16;
			}
		}
	}
}

// destination: bg_buf
void CompositeRGBA(char *bg_buf, int bg_w, int bg_h,
		int x, int y, char *img, int width, int height)
{
	int ix, iy;
	char *c, *bg, a;
	c = img;
	for (iy=0; iy<height; iy++) {
		bg = (char*)bg_buf + (y+iy)*(bg_w * 4) + x*4;
		for (ix=0; ix<width; ix++) {
			a = c[3];
			png_composite(bg[0], c[0], a, bg[0]); // r
			png_composite(bg[1], c[1], a, bg[1]); // g
			png_composite(bg[2], c[2], a, bg[2]); // b
			c += 4;
			bg += 4;
		}
	}
}

s32 __Gui_DrawPngRA(void *img, u32 x, u32 y, int width, int height)
{
	IMGCTX   ctx = NULL;
	PNGUPROP imgProp;
	char *img_buf = NULL, *resize_buf = NULL;

	s32 ret;

	/* Select PNG data */
	ctx = PNGU_SelectImageFromBuffer(img);
	if (!ctx) {
		ret = -1;
		goto out;
	}

	/* Get image properties */
	ret = PNGU_GetImageProperties(ctx, &imgProp);
	if (ret != PNGU_OK) {
		ret = -1;
		goto out;
	}

	img_buf = memalign(32, imgProp.imgWidth * imgProp.imgHeight * 4);
	if (!img_buf) {
		ret = -1;
		goto out;
	}
	resize_buf = memalign(32, width * height * 4);
	if (!resize_buf) {
		ret = -1;
		goto out;
	}

	// decode
	ret = PNGU_DECODE_TO_COORDS_RGBA8(ctx, 0, 0, imgProp.imgWidth, imgProp.imgHeight, 255,
			imgProp.imgWidth, imgProp.imgHeight, img_buf);
	// resize
	ResizeRGBA(img_buf, imgProp.imgWidth, imgProp.imgHeight, resize_buf, width, height);
	// combine
	Video_CompositeRGBA(x, y, resize_buf, width, height);
	// draw
	Video_DrawRGBA(x, y, resize_buf, width, height);

	/* Success */
	ret = 0;

out:
	if (img_buf) free(img_buf);
	if (resize_buf) free(resize_buf);

	/* Free memory */
	if (ctx)
		PNGU_ReleaseImageContext(ctx);

	return ret;
}


static void RGBA_to_4x4(const unsigned char *src, void *dst,
		const unsigned int width, const unsigned int height)
{
	unsigned int block;
	unsigned int i, c, j;
	unsigned int ar, gb;
	unsigned char *p = (unsigned char*)dst;

	for (block = 0; block < height; block += 4) {
		for (i = 0; i < width; i += 4) {
			/* Alpha and Red */
			for (c = 0; c < 4; ++c) {
				for (ar = 0; ar < 4; ++ar) {
					j = ((i + ar) + ((block + c) * width)) * 4;
					/* Alpha pixels */
					//*p++ = 255;
					*p++ = src[j + 3];
					/* Red pixels */
					*p++ = src[j + 0];
				}
			}

			/* Green and Blue */
			for (c = 0; c < 4; ++c) {
				for (gb = 0; gb < 4; ++gb) {
					j = (((i + gb) + ((block + c) * width)) * 4);
					/* Green pixels */
					*p++ = src[j + 1];
					/* Blue pixels */
					*p++ = src[j + 2];
				}
			}
		} /* i */
	} /* block */
}

static void C4x4_to_RGBA(const unsigned char *src, unsigned char *dst,
		const unsigned int width, const unsigned int height)
{
	unsigned int block;
	unsigned int i, c, j;
	unsigned int ar, gb;
	unsigned char *p = (unsigned char*)src;

	for (block = 0; block < height; block += 4) {
		for (i = 0; i < width; i += 4) {
			/* Alpha and Red */
			for (c = 0; c < 4; ++c) {
				for (ar = 0; ar < 4; ++ar) {
					j = ((i + ar) + ((block + c) * width)) * 4;
					/* Alpha pixels */
					//*p++ = 255;
					dst[j + 3] = *p++;
					/* Red pixels */
					dst[j + 0] = *p++;
				}
			}

			/* Green and Blue */
			for (c = 0; c < 4; ++c) {
				for (gb = 0; gb < 4; ++gb) {
					j = (((i + gb) + ((block + c) * width)) * 4);
					/* Green pixels */
					dst[j + 1] = *p++;
					/* Blue pixels */
					dst[j + 2] = *p++;
				}
			}
		} /* i */
	} /* block */
}


GRRLIB_texImg LoadTextureXX(const unsigned char my_png[],
		int width, int height, void *dest)
{
	PNGUPROP imgProp;
	IMGCTX ctx = NULL;
	GRRLIB_texImg my_texture;
	int ret, width4, height4;
	char *buf1 = NULL, *buf2 = NULL;

	my_texture.data = NULL;
	my_texture.w = 0;
	my_texture.h = 0;
	ctx = PNGU_SelectImageFromBuffer(my_png);
	if (ctx == NULL) goto out;
	ret = PNGU_GetImageProperties(ctx, &imgProp);
	if (ret != PNGU_OK) goto out;
   
   	buf1 = memalign (32, imgProp.imgWidth * imgProp.imgHeight * 4);
	if (buf1 == NULL) goto out;

	PNGU_DecodeToRGBA8 (ctx, imgProp.imgWidth, imgProp.imgHeight, buf1, 0, 255);

	if (imgProp.imgWidth != width || imgProp.imgHeight != height) {
	   	buf2 = memalign (32, width * height * 4);
		if (buf2 == NULL) goto out;
		ResizeRGBA(buf1, imgProp.imgWidth, imgProp.imgHeight, buf2, width, height);
		free(buf1);
		buf1 = buf2;
		buf2 = NULL;
	}

	width4 = (width + 3) >> 2 << 2;
	height4 = (height + 3) >> 2 << 2;
	buf2 = memalign (32, width4 * height4 * 4);
	if (buf2 == NULL) goto out;

	PadRGBA(buf1, width, height, buf2, width4, height4);

	if (dest == NULL) {
		dest = memalign (32, width4 * height4 * 4);
	}

	RGBA_to_4x4((u8*)buf2, (u8*)dest, width4, height4);

	my_texture.data = dest;
	my_texture.w = width4;
	my_texture.h = height4;
	GRRLIB_FlushTex(my_texture);
	out:
	if (buf1) free(buf1);
	if (buf2) free(buf2);
	if (ctx) PNGU_ReleaseImageContext(ctx);
	return my_texture;
}



Mtx GXmodelView2D;

extern int GRRLIB_Init_VMode(GXRModeObj *a_rmode, void *fb0);
extern void GRRLIB_DrawSlice(f32 xpos, f32 ypos, GRRLIB_texImg tex,
		float degrees, float scaleX, f32 scaleY, u32 color,
		float x, float y, float w, float h);
extern void** _GRRLIB_GetXFB(int *cur_fb);
extern void _GRRLIB_Render();
extern void _GRRLIB_VSync();
extern void _GRRLIB_Exit();

extern struct discHdr *gameList;
extern s32 all_gameCnt;
extern s32 gameCnt, gameSelected, gameStart;

#include <unistd.h>
#include <ogc/lwp.h>
#include <ogc/cond.h>
#include <ogc/mutex.h>

#include "sys.h"

#define COVER_CACHE_DATA_SIZE 0x2000000 // 32MB
#define CS_IDLE     0
#define CS_LOADING  1
#define CS_PRESENT  2
#define CS_MISSING  3

struct Cover_State 
{
	char used;
	char id[8];
	int gid;
	GRRLIB_texImg tx;
	//int lru;
};

struct Game_State
{
	int request;
	int state; // idle, loading, present, missing
	int cid; // cache index
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


int ccache_init = 0;
int ccache_inv = 0;
static struct Cover_Cache ccache;

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
	int gid = ccache.cover[i].gid;
	if (*ccache.cover[i].id && gid >= 0) {
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

void* cache_thread(void *arg)
{
	int i, ret;
	char *id;
	void *img;
	void *buf;
	int cid;
	int rq_prio;
	//dbg_printf("thread started\n");

	while (!ccache.quit) {

		LWP_MutexLock(ccache.mutex);

		ccache.idle = 0;

		//dbg_printf("thread running\n");

		restart:

		ccache.restart = 0;
		
		for (rq_prio=1; rq_prio<=3; rq_prio++) {
			for (i=0; i<ccache.num_game; i++) {
				if (ccache.restart) goto restart;
				if (ccache.game[i].request < rq_prio) continue;
				// load
				ccache.game[i].state = CS_LOADING;
				id = (char*)gameList[i].id;
				//dbg_printf("thread processing %d %s\n", i, id);
				img = NULL;

				LWP_MutexUnlock(ccache.mutex);

				ret = Gui_LoadCover((u8*)id, &img, false);
				//sleep(2);//dbg

				if (ccache.quit) goto quit;

				LWP_MutexLock(ccache.mutex);

				if (ret && img) {
					cid = cache_alloc(id);
					if (cid < 0) {
						// should not happen
						free(img);
						ccache.game[i].state = CS_IDLE;
						goto end_request;
					}
					buf = ccache.data + ccache.csize * cid;

					LWP_MutexUnlock(ccache.mutex);

					ccache.cover[cid].tx = LoadTextureXX(img, COVER_WIDTH, COVER_HEIGHT, buf);
					free(img);
					if (ccache.quit) goto quit;

					LWP_MutexLock(ccache.mutex);

					if (ccache.cover[cid].tx.data == NULL) {
						cache_free(cid);
						goto noimg;
					}
					// mark
					STRCOPY(ccache.cover[cid].id, id);
					// check if req change
					if (ccache.game[i].request) {
						ccache.cover[cid].used = 1;
						ccache.cover[cid].gid = i;
						ccache.game[i].cid = cid;
						ccache.game[i].state = CS_PRESENT;
						//dbg_printf("Load OK %d %d %s\n", i, cid, id);

					} else {
						ccache.game[i].state = CS_IDLE;
					}
				} else {
					noimg:
					ccache.game[i].state = CS_MISSING;
					//dbg_printf("Load FAIL %d %s\n", i, id);
				}
				end_request:
				// request processed.
				ccache.game[i].request = 0;
			}
		}
		ccache.idle = 1;
		//dbg_printf("thread idle\n");
		// all processed. wait.
		while (!ccache.restart && !ccache.quit) {
			LWP_MutexUnlock(ccache.mutex);
			//dbg_printf("thread sleep\n");
			LWP_ThreadSleep(ccache.queue);
			//dbg_printf("thread wakeup\n");
			LWP_MutexLock(ccache.mutex);
		}
		ccache.restart = 0;
		LWP_MutexUnlock(ccache.mutex);
	}

	quit:
	ccache.quit = 0;

	return NULL;
}

void cache_request_1(int game_i, int rq_prio)
{
	int cid;
	if (game_i < 0 || game_i >= ccache.num_game) return;
	if (ccache.game[game_i].request) return;
	// check if already present
	if (ccache.game[game_i].state == CS_PRESENT) {
		cid = ccache.game[game_i].cid;
		ccache.cover[cid].used = 1;
		return;
	}
	// check if already missing
	if (ccache.game[game_i].state == CS_MISSING) {
		return;
	}
	// check if already cached
	cid = cache_find((char*)gameList[game_i].id);
	if (cid >= 0) {
		ccache.cover[cid].used = 1;
		ccache.cover[cid].gid = game_i;
		ccache.game[game_i].cid = cid;
		ccache.game[game_i].state = CS_PRESENT;
		return;
	}
	// add request
	ccache.game[game_i].request = rq_prio;
}

void cache_request(int game_i, int num, int rq_prio)
{
	int i, idle;
	// setup requests
	LWP_MutexLock(ccache.mutex);
	for (i=0; i<num; i++) {
		cache_request_1(game_i + i, rq_prio);
	}
	// restart thread
	ccache.restart = 1;
	idle = ccache.idle;
	LWP_MutexUnlock(ccache.mutex);
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

void cache_release_1(int game_i)
{
	int cid;
	if (game_i < 0 || game_i >= ccache.num_game) return;
	ccache.game[game_i].request = 0;
	if (ccache.game[game_i].state == CS_PRESENT) {
		// release if already cached
		cid = ccache.game[game_i].cid;
		ccache.cover[cid].used = 0; //--
		ccache.game[game_i].state = CS_IDLE;
	}
}

void cache_release(int game_i, int num)
{
	int i;
	// cancel requests
	LWP_MutexLock(ccache.mutex);
	for (i=0; i<num; i++) {
		cache_release_1(game_i + i);
	}
	LWP_MutexUnlock(ccache.mutex);
}

void cache_release_all()
{
	int i;
	// cancel all requests
	LWP_MutexLock(ccache.mutex);
	for (i=0; i<ccache.num_game; i++) {
		cache_release_1(i);
	}
	LWP_MutexUnlock(ccache.mutex);
}

void Cache_Init()
{
	int ret;

	if (ccache_init) return;
	ccache_init = 1;
	
	ccache.data = LARGE_memalign(32, COVER_CACHE_DATA_SIZE);
	ccache.width4 = (COVER_WIDTH + 3) >> 2 << 2;
	ccache.height4 = (COVER_HEIGHT + 3) >> 2 << 2;
	ccache.csize = ccache.width4 * ccache.height4 * 4;

	ccache.cover_alloc = ccache.num = COVER_CACHE_DATA_SIZE / ccache.csize;
	ccache.cover = calloc(sizeof(struct Cover_State), ccache.cover_alloc);
	ccache.lru = 0;

	ccache.game_alloc = all_gameCnt;
	ccache.num_game = gameCnt;
	ccache.game = calloc(sizeof(struct Game_State), ccache.game_alloc);

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
	}
	//dbg_printf("lwp ret: %d id: %x\n", ret, ccache.lwp);
}

void Cache_Invalidate()
{
	int i, new_num;
	if (!ccache_init) return;
	LWP_MutexLock(ccache.mutex);
	// clear requests
	memset(ccache.game, 0, sizeof(struct Game_State) * ccache.game_alloc);
	LWP_MutexUnlock(ccache.mutex);
	// wait till idle
	i = 100;
	while (!ccache.idle && i) {
		usleep(10000);
		i--;
	}
	// clear all
	memset(ccache.game, 0, sizeof(struct Game_State) * ccache.game_alloc);
	memset(ccache.cover, 0, sizeof(struct Cover_State) * ccache.cover_alloc);

	// resize if games added
	if (all_gameCnt > ccache.game_alloc) {
		void *buf, *old;
		ccache.game_alloc = all_gameCnt;
		buf = calloc(sizeof(struct Game_State), ccache.game_alloc);
		old = ccache.game;
		ccache.game = buf;
		free(old);
	}
	ccache.num_game = gameCnt;
	// check cover size
	ccache.width4 = (COVER_WIDTH + 3) >> 2 << 2;
	ccache.height4 = (COVER_HEIGHT + 3) >> 2 << 2;
	ccache.csize = ccache.width4 * ccache.height4 * 4;
	new_num = COVER_CACHE_DATA_SIZE / ccache.csize;
	if (new_num > ccache.cover_alloc) {
		void *buf, *old;
		ccache.cover_alloc = new_num;
		buf = calloc(sizeof(struct Cover_State), ccache.cover_alloc);
		old = ccache.cover;
		ccache.cover = buf;
		free(old);
	}
	ccache.num = new_num;
	
	ccache_inv = 1;
}

void Cache_Close()
{
	int i;
	if (!ccache_init) return;
	ccache_init = 0;
	LWP_MutexLock(ccache.mutex);
	ccache.quit = 1;
	LWP_MutexUnlock(ccache.mutex);
	LWP_ThreadSignal(ccache.queue);
	i = 10;
	while (ccache.quit && i) {
		usleep(10);
		LWP_ThreadSignal(ccache.queue);
		i--;
	}
	LWP_JoinThread(ccache.lwp, NULL);
	LWP_CloseQueue(ccache.queue);
	LWP_MutexDestroy(ccache.mutex);
	ccache.lwp   = LWP_THREAD_NULL;
	ccache.queue = LWP_TQUEUE_NULL;
	ccache.mutex = LWP_MUTEX_NULL;
	if (ccache.cover) free(ccache.cover);
	if (ccache.game) free(ccache.game);
	ccache.cover = NULL;
	ccache.game = NULL;
}

#if 1
void draw_Cache()
{
	unsigned int i, x, y, c, cid;

	for (i=0; i<ccache.num; i++) {
		x = 15 + (i % 10) * 2;
		y = 40 + (i / 10) * 2;
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

	for (i=0; i<ccache.num_game; i++) {
		x = 15 + (i % 10)*2;
		y = 100 + (i / 10)*2;
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
}
#endif

typedef struct Grid_State
{
	float x, y, w, h;
	float scale, sx, sy;
	float zoom_step;
	float angle;
	int gi, state;
	int img_x, img_y;
	int center_x, center_y;
	GRRLIB_texImg tx;
} Grid_State;

struct M2_texImg
{
	GRRLIB_texImg tx;
	int size;
};

struct M2_texImg t2_bg;
struct M2_texImg t2_bg_con;
struct M2_texImg t2_nocover;
GRRLIB_texImg tx_pointer;
GRRLIB_texImg tx_hourglass;
GRRLIB_texImg tx_star;
GRRLIB_texImg tx_cfont;
static int grr_init = 0;
static int grx_init = 0;
int spacing = 20;
int columns = 5;
int rows = 2;
int page_covers = 0; // covers per page
int page_i;          // current page index
int page_gi = -1;    // first visible game index on page
int page_visible;    // num visible covers
int visible_add_rows = 0; // additional visible rows on each side of page
int num_pages;
int game_select = -1;
#define GUI_STYLE_GRID 0
#define GUI_STYLE_FLOW 1
#define GUI_STYLE_FLOW_Z 2
int gui_style = GUI_STYLE_GRID;
float max_w, max_h;
float scroll_per_cover; // 1 cover w
float scroll_per_page;  // 1 page w
float scroll_max;   // full grid max scroll
float page_scroll = 0; // current scroll position
// 3 max sized grids (4*8)
// plus (5 visible_add_rows) * 2:
// 3 * 4*(8+5*2)
#define GRID_SIZE 216

int grid_alloc = 0;
int grid_covers = 0;
struct Grid_State *grid_state = NULL;
int text_y = 480-20*2+5; //BACKGROUND_HEIGHT-spacing*2+5;
// FLOW_Z camera
static float cam_f = 0.0f;
static float cam_dir = 1.0;
static float cam_z = -578.0F;
static Vector cam_look = {319.5F, 239.5F, 0.0F};

//#include <processor.h>

extern void *bg_buf_rgba;

void cache2_tex(struct M2_texImg *dest, GRRLIB_texImg *src)
{
	int src_size = src->w * src->h * 4;
	void *m2_buf;
	// data exists and big enough?
	if (dest->tx.data && dest->size >= src_size) {
		// overwrite data
		m2_buf = dest->tx.data;
	} else {
		// alloc new
		m2_buf = LARGE_memalign(32, src_size);
		dest->size = src_size;
	}
	memcpy(m2_buf, src->data, src_size);
	memcpy(&dest->tx, src, sizeof (GRRLIB_texImg));
	dest->tx.data = m2_buf;
	free(src->data);
	src->data = NULL;
	GRRLIB_FlushTex(dest->tx);
}

void Render()
{
	_GRRLIB_Render();
	GX_DrawDone();
	_GRRLIB_VSync();
	if (gui_style == GUI_STYLE_FLOW_Z) {
	    GX_SetZMode (GX_FALSE, GX_NEVER, GX_TRUE);
	}
}

void Gui_Init()
{
	if (CFG.gui == 0) return;
	if (grr_init) return;
	//GRRLIB_Init();
	VIDEO_WaitVSync();
	if (GRRLIB_Init_VMode(_Video_GetVMode(), _Video_GetFB())) {
		printf("Error GRRLIB init\n");
		sleep(1);
		return;
	}
	VIDEO_WaitVSync();
	grr_init = 1;
}


void Grx_Init()
{
	void *bg_img = NULL;
	int ret;
	GRRLIB_texImg tx_tmp;

	if (!grx_init) {
		tx_pointer = GRRLIB_LoadTexture(pointer);
		tx_hourglass = GRRLIB_LoadTexture(hourglass);
		tx_star = GRRLIB_LoadTexture(star_icon);

		// Load the image file and initilise the tiles for console font
		tx_cfont = GRRLIB_LoadTexture(console_font);
		GRRLIB_InitTileSet(&tx_cfont, 8, 8, 0);
	}

	if (!grx_init || ccache_inv) {

		// background gui
		tx_tmp.data = NULL;
		ret = Fat_ReadFile(CFG.bg_gui, &bg_img);
		if (ret) {
			tx_tmp = GRRLIB_LoadTexture(bg_img);
			free(bg_img);
			if (tx_tmp.data &&
				(tx_tmp.w != BACKGROUND_WIDTH || tx_tmp.h != BACKGROUND_HEIGHT))
			{
				free(tx_tmp.data);
				tx_tmp.data = NULL;
			}
		}
		if (tx_tmp.data == NULL) {
			tx_tmp = GRRLIB_LoadTexture(bg_gui);
		}
		cache2_tex(&t2_bg, &tx_tmp);

		// background console
		tx_tmp = GRRLIB_CreateEmptyTexture(BACKGROUND_WIDTH, BACKGROUND_HEIGHT);
		if (bg_buf_rgba) {
			RGBA_to_4x4((u8*)bg_buf_rgba, (u8*)tx_tmp.data,
				BACKGROUND_WIDTH, BACKGROUND_HEIGHT);
		}
		cache2_tex(&t2_bg_con, &tx_tmp);

		// nocover image
		void *img = NULL;
		tx_tmp.data = NULL;
		ret = Gui_LoadCover((u8*)"", &img, true);
		if (ret > 0 && img) {
			tx_tmp = LoadTextureXX(img, COVER_WIDTH, COVER_HEIGHT, NULL);
			free(img);
		}
		if (tx_tmp.data == NULL) {
			tx_tmp = LoadTextureXX(coverImg, COVER_WIDTH, COVER_HEIGHT, NULL);
		}
		cache2_tex(&t2_nocover, &tx_tmp);
	}

	ccache_inv = 0;
	grx_init = 1;
}

void tilt_pos(Vector *pos)
{
	if (gui_style == GUI_STYLE_FLOW_Z) {
		// tilt pos
		float deg_max = 45.0;
		float deg = cam_f * cam_dir * deg_max;
		float rad = DegToRad(deg);
		float zf;
		Mtx rot;

		pos->x -= cam_look.x;
		pos->y -= cam_look.y;

		guMtxRotRad(rot, 'y', rad);
		guVecMultiply(rot, pos, pos);

		zf = (cam_z + pos->z) / cam_z;
		pos->x /= zf;
		pos->y /= zf;

		pos->x += cam_look.x;
		pos->y += cam_look.y;
	}
}

void tilt_cam(Vector *cam)
{
	if (gui_style == GUI_STYLE_FLOW_Z) {
		// tilt cam
		float deg_max = 45.0;
		float deg = cam_f * cam_dir * deg_max;
		float rad = DegToRad(deg);
		Mtx rot;

		cam->x -= cam_look.x;
		cam->y -= cam_look.y;

		guMtxRotRad(rot, 'y', rad);
		guVecMultiply(rot, cam, cam);

		cam->x += cam_look.x;
		cam->y += cam_look.y;
	}
}

void set_camera(ir_t *ir, int enable)
{
	float hold_w = 50.0, out = 64; // same as get_scroll() params
	float sx;
	if (ir) {
		// is out?
		// allow x out of screen by pointer size
		if (ir->sy < 0 || ir->sy > BACKGROUND_HEIGHT
			|| ir->sx < -out || ir->sx > BACKGROUND_WIDTH+out)
		{
			if (cam_f > 0.0) cam_f -= 0.02;
			if (cam_f < 0.0) cam_f = 0.0;
		} else {
			sx = ir->sx;
			if (sx < 0.0) sx = 0.0;
			if (sx > 640.0) sx = 640.0;
			sx = sx - 320.0;
			// check hold position
			if (sx < 0) {
				cam_dir = -1.0;
				sx = -sx;
			} else {
				cam_dir = 1.0;
			}
			sx -= hold_w;
			if (sx < 0) sx = 0;
			cam_f = sx / (320.0 - hold_w);
		}
	}

	if (gui_style == GUI_STYLE_GRID || gui_style == GUI_STYLE_FLOW) {
		enable = 0;
	}
	if (enable == 0) {
		// 2D
		Mtx44 perspective;
		guOrtho(perspective,0, 479, 0, 639, 0, 300.0F);
		GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);

		guMtxIdentity(GXmodelView2D);
		guMtxTransApply (GXmodelView2D, GXmodelView2D, 0.0F, 0.0F, -50.0F);
		GX_LoadPosMtxImm(GXmodelView2D, GX_PNMTX0);
	} else { // (gui_style == GUI_STYLE_FLOW_Z)
		// 3D
		Mtx44 perspective;
		guPerspective(perspective, 45, 4.0/3.0, 0.1F, 2000.0F);
		GX_LoadProjectionMtx(perspective, GX_PERSPECTIVE);

		//Vector cam  = {319.5F, 239.5F, -578.0F};
		//Vector look = {319.5F, 239.5F, 0.0F};
		Vector up   = {0.0F, -1.0F, 0.0F};
		Vector cam  = cam_look;
		cam.z = cam_z;

		// tilt cam
		tilt_cam(&cam);

		guLookAt(GXmodelView2D, &cam, &up, &cam_look);
		GX_LoadPosMtxImm(GXmodelView2D, GX_PNMTX0);
	}
	if (gui_style == GUI_STYLE_FLOW_Z) {
	    GX_SetZMode (GX_FALSE, GX_NEVER, GX_TRUE);
	}
}

void draw_background()
{
	set_camera(NULL, 0);
	GRRLIB_DrawImg(0, 0, t2_bg.tx, 0, 1, 1, 0xFFFFFFFF);
	set_camera(NULL, 1);
}

void draw_pointer(ir_t *ir)
{
	// reset camera
	set_camera(NULL, 0);
	// draw pointer
	GRRLIB_DrawImg(ir->sx - tx_pointer.w / 2, ir->sy - tx_pointer.h / 2,
			tx_pointer, ir->angle, 1, 1, 0xFFFFFFFF);
}

void Wpad_IR(int chan, struct ir_t *ir)
{
	WPAD_IR(chan, ir);
	// allow out of screen
	ir->sx -= 160;
	ir->sy -= 220;
}


///// GRID stuff


void grid_allocate()
{
	if (grid_alloc < gameCnt || grid_alloc == 0) {
		grid_alloc = gameCnt;
		if (grid_alloc < GRID_SIZE) grid_alloc = GRID_SIZE;
		grid_state = realloc(grid_state, grid_alloc * sizeof(Grid_State));
		if (grid_state == NULL) {
			printf("FATAL: alloc grid(%d)\n", grid_alloc);
			sleep(5);
			Sys_Exit();
		}
	}
}

void reset_grid_1(struct Grid_State *GS)
{
	GS->gi = -1;
	GS->x = 0;
	GS->y = 0;
	GS->w = 0;
	GS->h = 0;
	GS->zoom_step = 0;
	GS->angle = 0;
}

void reset_grid()
{
	int i;
	for (i=0; i<grid_covers; i++) {
		reset_grid_1(&grid_state[i]);
	}
}

void update_grid_state(struct Grid_State *GS)
{
	int gi = GS->gi;
	if (gi < 0) return;
	GS->state = ccache.game[gi].state;
	if (GS->state == CS_PRESENT) {
		GS->tx = ccache.cover[ccache.game[gi].cid].tx;
		GS->angle = 0.0;
	} else if (GS->state == CS_IDLE) {
		if (gi > 0 && ccache.game[gi-1].state == CS_LOADING) goto loading;
		GS->tx = tx_hourglass;
		GS->angle = 0.0;
	} else if (GS->state == CS_LOADING) {
		loading:
		GS->tx = tx_hourglass;
		GS->angle += 6.0;
		if (GS->angle > 180.0) GS->angle -= 360.0;
	} else { // CS_MISSING
		GS->tx = t2_nocover.tx;
		GS->angle = 0.0;
	}
	GS->sx = (float)COVER_WIDTH / (float)GS->tx.w * GS->scale;
	GS->sy = (float)COVER_HEIGHT / (float)GS->tx.h * GS->scale;
	GS->img_x = GS->center_x - GS->tx.w / 2;
	GS->img_y = GS->center_y - GS->tx.h / 2;
}

void calc_scroll_range()
{
	int corner_x;
	max_w = (float)((BACKGROUND_WIDTH-spacing*3) / columns - spacing);
	max_h = (float)((BACKGROUND_HEIGHT-spacing*4) / rows - spacing);
	scroll_per_cover = max_w + spacing;
	scroll_per_page = scroll_per_cover * columns;
	// last corner_x
	//corner_x = spacing*2 + (int)(max_w+spacing) * ix;
	corner_x = spacing*2 + (int)(max_w+spacing) * ((grid_covers-1) / rows);
	scroll_max = corner_x + max_w + spacing*2 - BACKGROUND_WIDTH;
	if (scroll_max < 0) scroll_max = 0;
}

float get_scroll_pos(int gi)
{
	return scroll_per_cover * ((int)(gi / rows));
}

void grid_calc_i(int game_i)
{
	int i, gi;
	int ix, iy;
	int corner_x = 0, corner_y;
	struct Grid_State *GS;

	calc_scroll_range();

	for (i=0; i<grid_covers; i++) {

		GS = &grid_state[i];
		gi = game_i + i;
		GS->gi = gi;
		if (gi >= gameCnt) {
			GS->gi = -1;
			break;
		}

		if (gui_style == GUI_STYLE_GRID) {
			ix = i % columns;
			iy = i / columns;
		} else {
			ix = i / rows;
			iy = i % rows;
		}
		corner_x = spacing*2 + (int)(max_w+spacing) * ix;
		corner_y = spacing*2 + (int)(max_h+spacing) * iy;
		GS->center_x = corner_x + (int)(max_w / 2);
		GS->center_y = corner_y + (int)(max_h / 2);
	}
}


void grid_calc()
{
	if (gui_style == GUI_STYLE_GRID) {
		grid_calc_i(page_gi);
	} else { // (gui_style == GUI_STYLE_FLOW) {
		grid_calc_i(0);
	}
}


void grid_shift_state(int gi_shift)
{
	int i, j, dir, start, end;
	if (gi_shift > 0) {
		dir = 1;
		start = 0;
		end = grid_covers;
	} else {
		dir = -1;
		start = grid_covers - 1;
		end = -1;
	}
	for (i=start; i!=end; i+=dir) {
		j = i + gi_shift;
		if (j<0 || j >= grid_covers) {
			reset_grid_1(&grid_state[i]);
		} else {
			grid_state[i] = grid_state[j];
		}
	}
}

void draw_grid_1(struct Grid_State *GS, float screen_x, float screen_y)
{
	bool miss, loading;
	float sx, sy;
	u32 color;

	if (GS->gi < 0 || GS->gi >= gameCnt) return;

	miss = true;
	loading = false;
	if (GS->state == CS_PRESENT) miss = false;
	if (GS->state == CS_IDLE || GS->state == CS_LOADING) loading = true;

	if (!loading) {
		sx = GS->sx;
		sy = GS->sy;
	} else {
		sx = sy = 0.5 + GS->zoom_step/5;
	}

	// dim unselected, light up selected
	if (game_select >=0 && GS->gi == game_select) {
		color = 0xFFFFFFFF;
	} else {
		color = 0xDDDDDDFF;
	}

	#if 0
	if (GS->gi == page_gi) {
		GRRLIB_Rectangle(screen_x + GS->center_x-max_w/2-spacing,
				screen_y + GS->center_y-max_h/2-spacing,
				max_w+spacing*2, max_h+spacing*2, 0x0000FFFF, 1);
	}
	if (GS->gi == page_gi + page_visible - 1) {
		GRRLIB_Rectangle(screen_x + GS->center_x-max_w/2-spacing,
				screen_y + GS->center_y-max_h/2-spacing,
				max_w+spacing*2, max_h+spacing*2, 0xFF0000FF, 1);
	}
	#endif

	GRRLIB_DrawImg(screen_x + GS->img_x, screen_y + GS->img_y,
		GS->tx, GS->angle, sx, sy, color);

	// favorite
	if (is_favorite(gameList[GS->gi].id)) {
		float center_x, center_y;
		float star_center_x, star_center_y;
		if (loading) {
			center_x = GS->img_x + GS->tx.w / 2;
			center_y = GS->img_y + GS->tx.h / 2;
			star_center_x = center_x + GS->tx.w/2 * sx;
			star_center_y = center_y - GS->tx.h/2 * sy;
		} else {
			center_x = GS->img_x + COVER_WIDTH/2;
			center_y = GS->img_y + COVER_HEIGHT/2;
			star_center_x = center_x + (COVER_WIDTH/2 - tx_star.w/2) * sx;
			star_center_y = center_y - (COVER_HEIGHT/2 - tx_star.h/2) * sy;
		}
		float star_x = star_center_x - tx_star.w/2;
		float star_y = star_center_y - tx_star.h/2;
		GRRLIB_DrawImg( screen_x + star_x, screen_y + star_y,
			tx_star, 0, sx, sy, color);
	}
	
	if (miss) {
		//if (!loading)
		GRRLIB_Rectangle(screen_x + GS->center_x-16-2, screen_y + GS->center_y+35-2,
			4*8+4, 8+4, 0xFFFFFFFF, 1);
		GRRLIB_Printf(screen_x + GS->center_x-16, screen_y + GS->center_y+35, tx_cfont,
			0x000000FF, 1.0, "%.4s", gameList[GS->gi].id);
	}
}

void draw_grid(int selected, float screen_x, float screen_y)
{
	int i;
	struct Grid_State *GS, *post = NULL;
	
	for (i=0; i<grid_covers; i++) {
		GS = &grid_state[i];
		if (GS->gi < 0) break;
		if (GS->gi == selected) {
			post = GS;
		} else {
			draw_grid_1(GS, screen_x, screen_y);
		}
	}
	if (post) {
		draw_grid_1(post, screen_x, screen_y);
	}
}

void draw_grid_sel(int selected, float screen_x, float screen_y)
{
	int i;
	struct Grid_State *GS;
	
	for (i=0; i<grid_covers; i++) {
		GS = &grid_state[i];
		if (GS->gi == selected) {
			draw_grid_1(GS, screen_x, screen_y);
		}
	}
}

void grid_draw(int selected)
{
	draw_grid(selected, -page_scroll, 0);
}

bool is_over(struct Grid_State *GS, ir_t *ir, float screen_x, float screen_y)
{
	bool over = false;
	float ir_sx = ir->sx;
	float ir_sy = ir->sy;
	float x1, y1, x2, y2;
	float x, y, w, h;

	if (GS->w > 0 && GS->h > 0) {
		//over = GRRLIB_PtInRect(GS->x, GS->y, GS->w, GS->h, ir_sx, ir_sy);
		x1 = GS->x;
		y1 = GS->y;
		x2 = GS->x + GS->w;
		y2 = GS->y + GS->h;
	} else {
		float corner_x = GS->center_x - (int)(max_w / 2);
		float corner_y = GS->center_y - (int)(max_h / 2);
		//over = GRRLIB_PtInRect(corner_x, corner_y, max_w, max_h, ir_sx, ir_sy);
		x1 = corner_x;
		y1 = corner_y;
		x2 = corner_x + max_w;
		y2 = corner_y + max_h;
	}
	x1 += screen_x;
	x2 += screen_x;
	y1 += screen_y;
	y2 += screen_y;
	if (gui_style == GUI_STYLE_FLOW_Z) {
		Vector p1 = { x1, y1, 0.0 };
		Vector p2 = { x2, y1, 0.0 };
		Vector p3 = { x1, y2, 0.0 };
		Vector p4 = { x2, y2, 0.0 };
		tilt_pos(&p1);
		tilt_pos(&p2);
		tilt_pos(&p3);
		tilt_pos(&p4);
		x1 = p1.x;
		y1 = MIN(p1.y, p2.y);
		x2 = p2.x;
		y2 = MAX(p3.y, p4.y);
		/*set_camera(NULL, 0);
		GRRLIB_Rectangle(x1-10, y1-10, 20, 20, 0xFF0000FF, 1);
		set_camera(NULL, 1);*/
	}
	x = x1;
	y = y1;
	w = x2 - x1;
	h = y2 - y1; 

	over = GRRLIB_PtInRect(x, y, w, h, ir_sx, ir_sy);
	return over;
}

// return selected
int grid_update_state_s(ir_t *ir, float screen_x, float screen_y)
{
	int i, selected = -1;
	float zoom;
	bool over;
	struct Grid_State *GS;

	for (i=0; i<grid_covers; i++) {

		GS = &grid_state[i];
		if (GS->gi < 0 || GS->gi >= gameCnt) break;

		if (ir) {
			over = is_over(GS, ir, screen_x, screen_y);
		} else {
			over = false;
		}
		// zoom
		if (over) {
			if (GS->zoom_step < 1) GS->zoom_step += 0.2;
			if (GS->zoom_step > 1) GS->zoom_step = 1;
			selected = GS->gi;
		} else {
			if (GS->zoom_step > 0) GS->zoom_step -= 0.05;
			if (GS->zoom_step < 0) GS->zoom_step = 0;
		}
		zoom = (float)spacing * 1.8 * (GS->zoom_step);
		// scale to fit
		GS->scale = max_w / (float)COVER_WIDTH;
		// does H fit?
		if ((float)COVER_HEIGHT * GS->scale <= max_h) {
			// yes, adjust zoom
			GS->scale = (max_w+zoom) / (float)COVER_WIDTH;
		} else {
			// no, fit H instead
			GS->scale = (max_h+zoom) / (float)COVER_HEIGHT;
		}

		update_grid_state(GS);

		// adjust coords
		GS->w = (float)GS->tx.w * GS->sx;
		GS->h = (float)GS->tx.h * GS->sy;
		GS->x = GS->center_x - (int)(GS->w / 2);
		GS->y = GS->center_y - (int)(GS->h / 2);
	}
	return selected;
}

int grid_update_state(ir_t *ir)
{
	// update game cover state
	return grid_update_state_s(ir, -page_scroll, 0);
}

// cap scroll
int update_scroll()
{
	int cap = 0;
	if (page_scroll > scroll_max) {
		page_scroll = scroll_max;
		cap = 1;
	}
	if (page_scroll < 0) {
		page_scroll = 0;
		cap = 1;
	}
	if (gui_style == GUI_STYLE_GRID) {
		page_scroll = 0;
	}
	return cap;
}

int get_first_visible()
{
	return (int)(page_scroll / scroll_per_cover) * rows;
}

void update_visible()
{
	// first visible, num visible
	if (gui_style == GUI_STYLE_GRID) {
		page_visible = page_covers;
	} else { // (gui_style == GUI_STYLE_FLOW) {
		page_gi = get_first_visible() - rows * visible_add_rows;
		if (page_gi < 0) page_gi = 0;
		page_visible = page_covers + rows * visible_add_rows * 2;
	}
}

void cache_visible()
{
	// todo: request only on change?
	cache_release_all();
	cache_request(page_gi, page_visible, 1);
}

// cap page_gi and calc page_i
void update_page_i()
{
	if (page_gi >= gameCnt) page_gi = gameCnt - 1;
	if (page_gi < 0) page_gi = 0;
	if (gui_style == GUI_STYLE_GRID) {
		page_i = page_gi / page_covers;
	} else { // if (gui_style == GUI_STYLE_FLOW) {
		// round up
		page_i = (page_scroll + scroll_per_page - scroll_per_cover/2) / scroll_per_page;
	}
	if (page_i >= num_pages) page_i = num_pages - 1;
	if (page_i < 0) page_i = 0;
}

int grid_update_nocache(ir_t *ir)
{
	// cap scroll
	update_scroll(); // dep: scroll_max
	update_visible();
	update_page_i();
	return grid_update_state(ir);
}

int grid_update_all(ir_t *ir)
{
	// cap scroll
	update_scroll(); // dep: scroll_max
	update_visible();
	cache_visible();
	update_page_i();
	return grid_update_state(ir);
}


float tran_f(float f1, float f2, float step)
{
	float f;
	f = f1 + (f2 - f1) * step;
	return f;
}

#define TRAN_F(F1, F2, S) F1 = tran_f(F1, F2, S)

void grid_transit(float screen_x, float screen_y, int selected, float tran)
{
	int i;
	struct Grid_State *GS;
	float dest_x, dest_y, dest_sx, dest_sy;
	
	for (i=0; i<grid_covers; i++) {
		GS = &grid_state[i];
		if (GS->gi < 0) break;
		if (GS->gi != selected) continue;

		// if loading, don't transit, leave it where it is
		if (GS->state == CS_IDLE || GS->state == CS_LOADING) break;
		
		// is missing force present to avoid printing game id
		if (GS->state == CS_MISSING) GS->state = CS_PRESENT;

		dest_x = COVER_XCOORD + page_scroll;
		dest_y = COVER_YCOORD;
		// COVER_WIDTH, COVER_HEIGHT
		dest_sx = 1;
		dest_sy = 1;
		//GS->img_x += (dest_x - GS->img_x) * tran;
		//GS->img_y += (dest_y - GS->img_y) * tran;
		TRAN_F( GS->img_x, dest_x, tran );
		TRAN_F( GS->img_y, dest_y, tran );
		GS->img_x -= (int)screen_x;
		GS->img_y -= (int)screen_y;
		TRAN_F( GS->center_x, dest_x + COVER_WIDTH/2, tran );
		TRAN_F( GS->center_y, dest_y + COVER_HEIGHT/2, tran );
		GS->center_x -= (int)screen_x;
		GS->center_y -= (int)screen_y;
		//GS->sx += (dest_sx - GS->sx) * tran;
		//GS->sy += (dest_sy - GS->sy) * tran;
		TRAN_F( GS->sx, dest_sx, tran );
		TRAN_F( GS->sy, dest_sy, tran );
	}
}

void draw_grid_t(float screen_x, float screen_y, int game_i, ir_t *ir, float tran)
{
	grid_calc();
	grid_update_nocache(NULL);
	grid_transit(screen_x, screen_y, gameSelected, tran);
	draw_grid(-1, screen_x-page_scroll, screen_y);
}

void transition_scroll(int direction, int grid_i)
{
	int tran_steps = 15;
	int i, j;
	float step, tran;
	ir_t ir;
	for (i=0; i<=tran_steps; i++) {
		if (direction > 0) j = i; else j = tran_steps - i;
		tran = (float)j / (float)tran_steps;
		step = (float)BACKGROUND_HEIGHT / (float)tran_steps * (float)j;
		WPAD_ScanPads();
		Wpad_IR(WPAD_CHAN_0, &ir);
		GRRLIB_DrawSlice(0, 0, t2_bg_con.tx, 0, 1, 1, 0xFFFFFFFF,
				0, step, BACKGROUND_WIDTH, BACKGROUND_HEIGHT - step);
		GRRLIB_DrawSlice(0, BACKGROUND_HEIGHT - step, t2_bg.tx, 0, 1, 1, 0xFFFFFFFF,
				0, 0, BACKGROUND_WIDTH, step);
		draw_grid_t(0, BACKGROUND_HEIGHT - step, grid_i, NULL, 1-tran);
		draw_pointer(&ir);
		Render();
	}
}

void transition_fade(int direction, int grid_i)
{
	int tran_steps = 15;
	int i, j, alpha;
	float tran;
	u32 color;
	ir_t ir;
	for (i=0; i<=tran_steps; i++) {
		if (direction > 0) j = i; else j = tran_steps - i;
		tran = (float)j / (float)tran_steps;
		alpha = 255 * j / tran_steps;
		WPAD_ScanPads();
		Wpad_IR(WPAD_CHAN_0, &ir);

		color = 0xFFFFFF00 | alpha;
		GRRLIB_DrawImg(0, 0, t2_bg.tx, 0, 1, 1, color);

		draw_grid_t(0, 0, grid_i, NULL, 1-tran);

		color = 0xFFFFFF00 | (255-alpha);
		GRRLIB_DrawImg(0, 0, t2_bg_con.tx, 0, 1, 1, color);

		draw_grid_sel(gameSelected, -page_scroll, 0);
		
		draw_pointer(&ir);
		Render();
	}
}

void transition(int direction, int grid_i)
{
	cache_request(gameSelected, 1, 1);
	if (CFG.gui_transit == 0) {
		transition_scroll(direction, grid_i);
	} else {
		transition_fade(direction, grid_i);
	}
}


void grid_set_style(int style, int r)
{
	gui_style = style;
	rows = r;
	switch (rows) {
		case 1: columns = 4; break;
		case 2: columns = 5; break;
		case 3: columns = 6; break;
		default:
		case 4: columns = 8; break;
	}
	page_covers = columns * rows;
	grid_covers = columns * rows;
	visible_add_rows = 0;
	if (gui_style == GUI_STYLE_FLOW) {
		grid_covers = gameCnt;
		visible_add_rows = 2;
	}
	if (gui_style == GUI_STYLE_FLOW_Z) {
		grid_covers = gameCnt;
		visible_add_rows = 6;
	}
	grid_allocate();
	num_pages = (gameCnt + page_covers - 1) / page_covers;
}

void grid_change_style(int style, int r)
{
	if (style < 0 || style > 2) return;
	if (r < 1 || r > 4) return;

	// save index for alignment
	int mid_gi;   // index to the game in the middle of the page
	int first_gi; // first really visible 
	if (gui_style == GUI_STYLE_GRID) {
		first_gi = page_gi;
	} else { // GUI_STYLE_FLOW
		// idx to column 2
		//first_gi = page_gi + rows * visible_add_rows;
		first_gi = get_first_visible();
	}
	mid_gi = first_gi + page_covers/2 - 1;

	grid_set_style(style, r);

	// align to previous index
	if (gui_style == GUI_STYLE_GRID) {
		// cap scroll
		update_scroll();
		// align to previous middle
		page_i = mid_gi / page_covers;
		if (page_i >= num_pages) page_i = num_pages - 1;
		page_gi = page_i * page_covers;
		// cap page indexes
		update_page_i();
	} else { //if (gui_style == GUI_STYLE_FLOW) {
		// recalc, to get scroll_per_cover
		grid_calc();
		// align to first visible
		page_scroll = get_scroll_pos(first_gi);
		// cap scroll
		update_scroll();
		// get first visible
		update_visible();
		// get page number
		update_page_i();
	}
	// camera
	if (gui_style == GUI_STYLE_FLOW_Z) {
		// 3D
		set_camera(NULL, 1);
	} else {
		// 2D
		set_camera(NULL, 0);
	}
	reset_grid();
}

void grid_change_rows(int r)
{
	int new_style, new_rows;
	if (r > 4) {
		new_style = (gui_style + 1) % 3;
		new_rows = rows;
	} else if (r < 1) {
		//new_style = gui_style - 1;
		//if (new_style < 0) new_style = 2;
		new_style = (gui_style + 1) % 3;
		new_rows = rows;
	} else {
		new_style = gui_style;
		new_rows = r;
	}
	grid_change_style(new_style, new_rows);
}

void _align_grid(
		struct Grid_State *grid, int gi, int n,
		struct Grid_State *grid1, int gi1, int n1,
		struct Grid_State *grid2, int gi2, int n2)
{
	int i, j1, j2;
	j1 = gi - gi1;
	j2 = gi - gi2;
	for (i=0; i<n; i++,j1++,j2++) {
		if (j1<0) {
			if (j2>=0 && j2<n2) {
				grid[i] = grid2[j2];
				if (gui_style == GUI_STYLE_GRID) {
					grid[i].center_y = -COVER_HEIGHT;
					grid[i].img_y = -COVER_HEIGHT;
				} else {
					grid[i].center_x = -COVER_WIDTH;
					grid[i].img_x = -COVER_WIDTH;
				}
			}
		} else if (j1<n1) {
			grid[i] = grid1[j1];
		} else { // j >= n1
			if (j2>=0 && j2<n2) {
				grid[i] = grid2[j2];
				if (gui_style == GUI_STYLE_GRID) {
					grid[i].center_y = BACKGROUND_HEIGHT + COVER_HEIGHT;
					grid[i].img_y = BACKGROUND_HEIGHT + COVER_HEIGHT;
				} else {
					grid[i].center_x = BACKGROUND_WIDTH + COVER_WIDTH;
					grid[i].img_x = BACKGROUND_WIDTH + COVER_WIDTH;
				}
			}
		}
	}
}

int align_grid(struct Grid_State *grid1, int gi1, int n1,
		struct Grid_State *grid2, int gi2, int n2)
{
	struct Grid_State a_grid1[GRID_SIZE];
	struct Grid_State a_grid2[GRID_SIZE];
	int gi, gl, n;

	gi = MIN(gi1, gi2); // first
	gl = MAX(gi1+n1, gi2+n2); // last
	n = gl - gi; // num
	if (n > GRID_SIZE) n = GRID_SIZE;

	memset(a_grid1, 0, sizeof(a_grid1));
	memset(a_grid2, 0, sizeof(a_grid2));

	_align_grid(a_grid1, gi, n, grid1, gi1, n1, grid2, gi2, n2);
	_align_grid(a_grid2, gi, n, grid2, gi2, n2, grid1, gi1, n1);

	memcpy(grid1, a_grid1, n * sizeof(struct Grid_State));
	memcpy(grid2, a_grid2, n * sizeof(struct Grid_State));
	return n;
}

void transit_grid2(struct Grid_State *grid1, struct Grid_State *grid2, int n, float step)
{
	int i;
	struct Grid_State *GS;
	for (i=0; i<n; i++) {
		update_grid_state(&grid1[i]);
		update_grid_state(&grid2[i]);
		GS = &grid_state[i];
		*GS = grid2[i];

#define TRAN_GRID_F(V) GS->V = tran_f(grid1[i].V, grid2[i].V, step)
#define TRAN_GRID_I(V) GS->V = (int)tran_f((float)grid1[i].V, (float)grid2[i].V, step)

		TRAN_GRID_F(sx);
		TRAN_GRID_F(sy);
		TRAN_GRID_I(img_x);
		TRAN_GRID_I(img_y);
		TRAN_GRID_I(center_x);
		TRAN_GRID_I(center_y);
	}
}

void grid_move(struct Grid_State *grid, int gnum, float x, float y)
{
	int i;
	struct Grid_State *GS;
	for (i=0; i<gnum; i++) {
		GS = &grid[i];
		GS->img_x += (int)x;
		GS->img_y += (int)y;
		GS->center_x += (int)x;
		GS->center_y += (int)y;
	}
}

void grid_copy_vis(struct Grid_State *grid, int *gi, int *gnum)
{
	*gi = page_gi;
	if (gui_style == GUI_STYLE_GRID) {
		if (page_visible < *gnum) *gnum = page_visible;
		memcpy(grid, grid_state, *gnum * sizeof(Grid_State));
	} else {
		int my_gi;
		// take a bit more than visible.
		// take GRID_SIZE/2 from middle of visible
		my_gi = (page_gi + page_visible / 2) - GRID_SIZE / 4;
		// cap start
		if (my_gi < 0) my_gi = 0;
		// cap size to GRID_SIZE/2
		if (*gnum > GRID_SIZE/2) *gnum = GRID_SIZE/2;
		// cap to actual numebr of covers
		if (my_gi + *gnum > grid_covers) {
			*gnum = grid_covers -my_gi;
		}
		// safety check
		if (*gnum < 0) {
			*gnum = 0;
		}
		*gi = my_gi;
		memcpy(grid, grid_state + my_gi, *gnum * sizeof(Grid_State));
		grid_move(grid, *gnum, -page_scroll, 0);
	}
}

void print_style(int change)
{
	char *style_name[] = { "grid", "flow", "flow-z" };
	//static int color = 0x000000FF;
	static int color = 0x00000000;
	if (change) {
		//color = 0x000000FF;
		color = CFG.gui_text_color;
	} else {
		int alpha = color & 0xFF;
		if (alpha > 0) alpha -= 3;
		if (alpha < 0) alpha = 0;
		color = (color & 0xFFFFFF00) | alpha;
	}
	// reset camera
	set_camera(NULL, 0);
	// print style
	GRRLIB_Printf(BACKGROUND_WIDTH - spacing*2 - 50 - 90, text_y,
		tx_cfont, color, 1, "[%s %d]",
		style_name[gui_style], rows);
}

void print_title(int selected)
{
	text_y = BACKGROUND_HEIGHT-spacing*2+5;
	// style
	print_style(0);
	// page
	GRRLIB_Printf(BACKGROUND_WIDTH - spacing*2 - 50, text_y, tx_cfont,
		CFG.gui_text_color, 1, "%d / %d", page_i+1, num_pages);
	// title
	if (selected < 0) return;
	GRRLIB_Printf(spacing*2, text_y, tx_cfont,
		CFG.gui_text_color, 1, "%s", get_title(&gameList[selected]));
}


void transit_style(int r)
{
	int tran_steps = 15;
	float step;
	ir_t ir;
	int old_gi, new_gi;
	int old_n, new_n;
	int gi, gj, max_n, old_covers;
	int i;
	struct Grid_State grid1[GRID_SIZE], grid2[GRID_SIZE];

	reset_grid();
	grid_calc();
	grid_update_nocache(NULL);
	old_n = GRID_SIZE;
	grid_copy_vis(grid1, &old_gi, &old_n);

	grid_change_rows(r);

	new_gi = page_gi;
	reset_grid();
	grid_calc();
	grid_update_nocache(NULL);
	new_n = GRID_SIZE;
	grid_copy_vis(grid2, &new_gi, &new_n);

	gi = MIN(old_gi, new_gi);
	gj = MAX(old_gi+old_n, new_gi+new_n);
	max_n = gj - gi;

	cache_release_all();
	cache_request(gi, max_n, 1);

	align_grid(grid1, old_gi, old_n, grid2, new_gi, new_n);

	// transition
	old_covers = grid_covers;
	grid_covers = max_n;
	reset_grid();
	for (i=1; i<tran_steps; i++) {
		WPAD_ScanPads();
		Wpad_IR(WPAD_CHAN_0, &ir);
		draw_background();

		step = (float)i / (float)tran_steps;
		transit_grid2(grid1, grid2, max_n, step);

		draw_grid(-1, 0, 0);
		print_style(1);
		draw_pointer(&ir);
		Render();
	}
	grid_covers = old_covers;
	reset_grid();
	grid_calc();
	grid_update_all(NULL);

	//cache_release_all();
	//cache_request(page_i * grid_covers, grid_covers, 1);
}


void Repaint_ConBg()
{
	int fb;
	void **xfb;
	GRRLIB_texImg tx;
	void *img_buf;

	xfb = _GRRLIB_GetXFB(&fb);
	_Video_SetFB(xfb[fb^1]);
	Video_Clear(COLOR_BLACK);

	// background
	Video_DrawBg();

	// Cover
	if (CFG.covers == 0) return;
	if (gameCnt == 0) return;
	if (ccache.game[gameSelected].state == CS_PRESENT) {
		tx = ccache.cover[ccache.game[gameSelected].cid].tx;
	} else if (ccache.game[gameSelected].state == CS_MISSING) {
		tx = t2_nocover.tx;
	} else { // loading
		return;
	}
	img_buf = memalign(32, tx.w * tx.h * 4);
	if (img_buf == NULL) return;
	C4x4_to_RGBA(tx.data, img_buf, tx.w, tx.h);
	DrawCoverImg(gameList[gameSelected].id, img_buf, tx.w, tx.h);
	//Video_CompositeRGBA(COVER_XCOORD, COVER_YCOORD, img_buf, tx.w, tx.h);
	//Video_DrawRGBA(COVER_XCOORD, COVER_YCOORD, img_buf, tx.w, tx.h);
	free(img_buf); 
}

float get_scroll(ir_t *ir)
{
	float w2, sx, dir = -1, s = 0;
	float scroll_w, hold_w = 50, out = 64;
	float dx; // distance from hold
	float speed = 20;
	// if y out of screen, don't scroll
	if (ir->sy < 0 || ir->sy > BACKGROUND_HEIGHT) return 0;
	// allow x out of screen by pointer size
	if (ir->sx < -out || ir->sx > BACKGROUND_WIDTH+out) return 0;
	sx = ir->sx;
	// slow down if out of screen?
	//if (sx < 0) sx = -sx;
	//if (sx > BACKGROUND_WIDTH) sx = BACKGROUND_WIDTH - (sx-BACKGROUND_WIDTH);
	// cap
	if (sx < 0) sx = 0;
	if (sx > BACKGROUND_WIDTH) sx = BACKGROUND_WIDTH;
	// direction
	w2 = BACKGROUND_WIDTH / 2;
	scroll_w = w2 - hold_w;
	if (sx > w2) {
		sx = BACKGROUND_WIDTH - sx;
		dir = 1;
	}
	dx = w2 - sx;
	if (dx > hold_w) {
		// in scroll zone
		s = (dx - hold_w) / scroll_w;
		s = s * s * speed * dir;
	}
	return s;
}

void transition_page_grid(int gi_1, int gi_2)
{
	int tran_steps = 20;
	int i;
	float step, dir;
	ir_t ir;
	reset_grid();
	for (i=1; i<tran_steps; i++) {
		WPAD_ScanPads();
		Wpad_IR(WPAD_CHAN_0, &ir);
		if (gi_2 > gi_1) dir = 1; else dir = -1;
		step = (float)BACKGROUND_WIDTH / (float)tran_steps * (float)i;
		draw_background();
		// page 1
		grid_calc_i(gi_1);
		grid_update_nocache(NULL);
		draw_grid(-1, -step * dir, 0);
		// page 2
		grid_calc_i(gi_2);
		grid_update_nocache(NULL);
		draw_grid(-1, (BACKGROUND_WIDTH - step) * dir, 0);
		// done
		draw_pointer(&ir);
		Render();
	}
}

void transition_page_flow(float new_scroll)
{
	int i, tran_steps = 20;
	float step, dir;
	int end = 0;
	ir_t ir;
	if (new_scroll > page_scroll) {
		dir = 1;
	} else {
		dir = -1;
	}
	step = scroll_per_page / tran_steps * dir;
	for (i=0; i<tran_steps && !end; i++) {
		WPAD_ScanPads();
		Wpad_IR(WPAD_CHAN_0, &ir);
		draw_background();
		page_scroll += step;
		end = update_scroll();
		grid_update_nocache(&ir);
		grid_draw(-1);
		// done
		draw_pointer(&ir);
		Render();
	}
}

int page_change(int pg_change)
{
	int new_page = 0;
	if (gui_style == GUI_STYLE_GRID) {
		new_page = page_i + pg_change;
		if (new_page >= num_pages) new_page = num_pages - 1;
		if (new_page < 0) new_page = 0;
		if (new_page != page_i) {
			cache_request(new_page * grid_covers, grid_covers, 1);
			transition_page_grid(page_i * grid_covers, new_page * grid_covers);
			cache_release_all();
			cache_request(new_page * grid_covers, grid_covers, 1);
			page_i = new_page;
			page_gi = page_i * grid_covers;
			return 1;
		}
	} else { // if (gui_style == GUI_STYLE_FLOW) {
		int new_gi;
		float new_scroll;
		new_scroll = page_scroll + scroll_per_page * pg_change;
		//page_scroll = new_scroll;
		//update_scroll();
		new_gi = page_gi + page_covers * pg_change;
		cache_request(new_gi, page_visible, 1);
		transition_page_flow(new_scroll);
		update_visible();
		cache_visible();
		update_page_i();
		return 1;
	}
	return 0;
}

void page_move_to(int gi)
{
	if (gui_style == GUI_STYLE_GRID) {
		page_i = gi / grid_covers;
		page_gi = page_i * grid_covers;
	} else {
		float gi_scroll;
		calc_scroll_range();
		gi_scroll = get_scroll_pos(gi);
		// if not on page, center on screen 
		if (page_scroll < gi_scroll - scroll_per_page) {
			page_scroll = gi_scroll - scroll_per_page / 2;
		}
		if (page_scroll > gi_scroll) {
			page_scroll = gi_scroll - scroll_per_page / 2;
		}
	}
	update_scroll();
	update_page_i();
}

void grid_init(int game_sel)
{
	static int first_time = 1;
	if (first_time) {
		gui_style = CFG.gui_style;
		rows = CFG.gui_rows;
		first_time = 0;
	}
	grid_set_style(gui_style, rows);
	grid_allocate();
	// move page to currently selected
	page_move_to(game_sel);
	reset_grid();
	grid_calc();
	// update all params and cache current page covers
	grid_update_all(NULL);
}

int Gui_Mode()
{
	int buttons = 0;
	ir_t ir;
	int fr_cnt = 0;

	if (CFG.gui == 0) return 0;
	Grx_Init();

	Cache_Init();

	grid_init(gameSelected);

	__console_flush(0);
	VIDEO_Flush();
	VIDEO_WaitVSync();

	Gui_Init();

	game_select = -1;
	// transition
	transition(1, page_gi);


	while (1) {

		restart:

		//dbg_time1();

		buttons = Wpad_GetButtons();
		Wpad_IR(WPAD_CHAN_0, &ir);

		if (buttons & (WPAD_BUTTON_A | WPAD_BUTTON_MINUS)) {
			if (game_select >= 0) {
				gameSelected = game_select;
				break;
			}
		}
		if (buttons & (WPAD_BUTTON_B | WPAD_BUTTON_1)) {
			if (game_select >= 0) gameSelected = game_select;
			break;
		}
		if (buttons & WPAD_BUTTON_2) {
			extern void Switch_Favorites(bool enable);
			extern bool enable_favorite;
			cache_release_all();
			Switch_Favorites(!enable_favorite);
			ccache.num_game = gameCnt;
			//grid_init(page_gi);
			grid_init(0);
		}
		if (buttons & WPAD_BUTTON_HOME) {
			if (CFG.home == CFG_HOME_SCRSHOT) {
				char fn[200];
				extern bool Save_ScreenShot(char *fn, int size);
				Save_ScreenShot(fn, sizeof(fn));
			} else {
				break;
			}
		}
		if (buttons & WPAD_BUTTON_PLUS) {
			break;
		}

		if (buttons & WPAD_BUTTON_RIGHT) {
			if (page_change(1)) goto restart;
		}
		if (buttons & WPAD_BUTTON_LEFT) {
			if (page_change(-1)) goto restart;
		}

		if (buttons & WPAD_BUTTON_UP) {
			//grid_change_rows(rows+1);
			transit_style(rows+1);
			goto restart;
		}
		if (buttons & WPAD_BUTTON_DOWN) {
			//grid_change_rows(rows-1);
			transit_style(rows-1);
			goto restart;
		}

		// scroll flow style
		if (gui_style == GUI_STYLE_FLOW
			|| gui_style == GUI_STYLE_FLOW_Z) {
			page_scroll += get_scroll(&ir);
		}
		
		//_CPU_ISR_Disable(level);
		draw_background();

		grid_calc();
		game_select = grid_update_all(&ir);
		set_camera(&ir, 1);
		grid_draw(game_select);
		// title
		print_title(game_select);

		/*int ms = dbg_time2(NULL);
		GRRLIB_Printf(20, 20, tx_cfont, 0xFFFFFFFF, 1, "%3d ms: %d", fr_cnt, ms);
		draw_Cache();*/
		

		draw_pointer(&ir);
		//GRRLIB_Rectangle(fr_cnt % 640, 0, 5, 15, 0x000000FF, 1);

		//_CPU_ISR_Restore(level);
		//GRRLIB_Render();
		Render();

		fr_cnt++;
	}

	//transition(-1, page_i*grid_covers);
	transition(-1, page_gi);

	Repaint_ConBg();

	// restore previous framebuffer
	_Video_SyncFB();

	cache_release_all();

	return buttons;
}

void Gui_Close()
{
	if (CFG.gui == 0) return;
	if (!grr_init) return;
	Cache_Close();
	__console_flush(0);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	_GRRLIB_Exit(); 
	free(tx_pointer.data);
	free(tx_cfont.data);
	grr_init = 0;
}


