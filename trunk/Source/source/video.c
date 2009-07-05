#include <stdio.h>
#include <ogcsys.h>
#include <stdlib.h>

#include "sys.h"
#include "video.h"
#include "cfg.h"
#include "libpng/png.h"

/* Video variables */
static void *framebuffer = NULL;
static GXRModeObj *vmode = NULL;

extern s32 CON_InitTr(GXRModeObj *rmode, s32 conXOrigin,s32 conYOrigin,s32 conWidth,s32 conHeight, s32 bgColor);
void _Con_Clear(void);

void Con_Init(u32 x, u32 y, u32 w, u32 h)
{
	/* Create console in the framebuffer */
	if (CFG.console_transparent) {
		CON_InitTr(vmode, x, y, w, h, CONSOLE_BG_COLOR);
	} else {
		CON_InitEx(vmode, x, y, w, h);
	}
	_Con_Clear();
}

void _Con_Clear(void)
{
	DefaultColor();
	/* Clear console */
	printf("\x1b[2J");
	fflush(stdout);
}

void Con_Clear(void)
{
	__console_flush(0);
	_Con_Clear();
}

void Con_ClearLine(void)
{
	s32 cols, rows;
	u32 cnt;

	printf("\r");
	fflush(stdout);

	/* Get console metrics */
	CON_GetMetrics(&cols, &rows);

	/* Erase line */
	for (cnt = 1; cnt < cols; cnt++) {
		printf(" ");
		fflush(stdout);
	}

	printf("\r");
	fflush(stdout);
}

void Con_FgColor(u32 color, u8 bold)
{
	/* Set foreground color */
	printf("\x1b[%u;%um", color + 30, bold);
	fflush(stdout);
}

void Con_BgColor(u32 color, u8 bold)
{
	/* Set background color */
	printf("\x1b[%u;%um", color + 40, bold);
	fflush(stdout);
}

void Con_FillRow(u32 row, u32 color, u8 bold)
{
	s32 cols, rows;
	u32 cnt;

	/* Set color */
	printf("\x1b[%u;%um", color + 40, bold);
	fflush(stdout);

	/* Get console metrics */
	CON_GetMetrics(&cols, &rows);

	/* Save current row and col */
	printf("\x1b[s");
	fflush(stdout);

	/* Move to specified row */
	printf("\x1b[%u;0H", row);
	fflush(stdout);

	/* Fill row */
	for (cnt = 0; cnt < cols; cnt++) {
		printf(" ");
		fflush(stdout);
	}

	/* Load saved row and col */
	printf("\x1b[u");
	fflush(stdout);

	/* Set default color */
	Con_BgColor(0, 0);
	Con_FgColor(7, 1);
}

void Video_Configure(GXRModeObj *rmode)
{
	/* Configure the video subsystem */
	VIDEO_Configure(rmode);

	/* Setup video */
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();

	if (rmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();
}

void Video_SetMode(void)
{
	void *old_fb = NULL;
	/* Select preferred video mode */
	vmode = VIDEO_GetPreferredMode(NULL);

	// widescreen mode
	if (CFG.widescreen)	{
		vmode->viWidth = 678;
		vmode->viXOrigin = ((VI_MAX_WIDTH_NTSC - 678) / 2);
	}

	// keep old fb
	if (framebuffer) {
		old_fb = MEM_K1_TO_K0(framebuffer);
	}

	/* Allocate memory for the framebuffer */
	framebuffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));

	/* Configure the video subsystem */
	VIDEO_Configure(vmode);

	/* Setup video */
	VIDEO_SetNextFramebuffer(framebuffer);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();

	if (vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();

	/* Clear the screen */
	Video_Clear(COLOR_BLACK);

	// free old framebuffer if any
	if (old_fb) free(old_fb);
}

void Video_Clear(s32 color)
{
	VIDEO_ClearFrameBuffer(vmode, framebuffer, color);
}

void Video_DrawPng(IMGCTX ctx, PNGUPROP imgProp, u16 x, u16 y)
{
	PNGU_DECODE_TO_COORDS_YCbYCr(ctx, x, y, imgProp.imgWidth, imgProp.imgHeight, vmode->fbWidth, vmode->xfbHeight, framebuffer);
}

GXRModeObj* _Video_GetVMode()
{
	return vmode;
}

void* _Video_GetFB()
{
	return framebuffer;
}

void _Video_SetFB(void *fb)
{
	framebuffer = fb;
}

void _Video_SyncFB()
{
	VIDEO_SetNextFramebuffer(framebuffer);
	VIDEO_Flush();
	VIDEO_WaitVSync();
}

void FgColor(int color)
{
	Con_FgColor(color & 7, color > 7 ? 1 : 0);
}

void BgColor(int color)
{
	Con_BgColor(color & 7, color > 7 ? 1 : 0);
}

void DefaultColor()
{
	FgColor(CONSOLE_FG_COLOR);
	BgColor(CONSOLE_BG_COLOR);
}

#define BACKGROUND_WIDTH	640
#define BACKGROUND_HEIGHT	480

/** (from GRRLIB)
 * Make a PNG screenshot on the SD card.
 * @return True if every thing worked, false otherwise.
 */
bool ScreenShot(char *fname)
{
    IMGCTX pngContext;
    int ErrorCode = -1;
	void *fb = VIDEO_GetCurrentFramebuffer();

    if((pngContext = PNGU_SelectImageFromDevice(fname)))
    {
        ErrorCode = PNGU_EncodeFromYCbYCr(pngContext,
				BACKGROUND_WIDTH, BACKGROUND_HEIGHT, fb, 0);
        PNGU_ReleaseImageContext(pngContext);
    }
    return !ErrorCode;
}

void *bg_buf_rgba = NULL;
void *bg_buf_ycbr = NULL;

void Video_DrawBg()
{
	//Video_DrawRGBA(0, 0, bg_buf_rgba, vmode->fbWidth, vmode->xfbHeight);
	//Video_DrawRGBA(0, 0, bg_buf_rgba, BACKGROUND_WIDTH, BACKGROUND_HEIGHT);
	memcpy(framebuffer, bg_buf_ycbr, BACKGROUND_WIDTH * BACKGROUND_HEIGHT * 2);
}

s32 Video_SaveBg(void *img)
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

	if (bg_buf_rgba == NULL) {
		//bg_buf_rgba = malloc(vmode->fbWidth * vmode->xfbHeight * 4);
		//bg_buf_rgba = malloc(BACKGROUND_WIDTH * BACKGROUND_HEIGHT * 4);
		bg_buf_rgba = LARGE_memalign(32, BACKGROUND_WIDTH * BACKGROUND_HEIGHT * 4);
		if (!bg_buf_rgba) { ret = -1; goto out; }
	}

	if (bg_buf_ycbr == NULL) {
		//bg_buf_ycbr = malloc(BACKGROUND_WIDTH * BACKGROUND_HEIGHT * 2);
		bg_buf_ycbr = LARGE_memalign(32, BACKGROUND_WIDTH * BACKGROUND_HEIGHT * 2);
		if (!bg_buf_ycbr) { ret = -1; goto out; }
	}

	/* Decode image */
	//PNGU_DECODE_TO_COORDS_RGBA8(ctx, 0, 0, imgProp.imgWidth, imgProp.imgHeight, 255,
	//		vmode->fbWidth, vmode->xfbHeight, bg_buf_rgba);
	PNGU_DECODE_TO_COORDS_RGBA8(ctx, 0, 0, imgProp.imgWidth, imgProp.imgHeight, 255,
			BACKGROUND_WIDTH, BACKGROUND_HEIGHT, bg_buf_rgba);

	RGBA_to_YCbYCr(bg_buf_ycbr, bg_buf_rgba, BACKGROUND_WIDTH, BACKGROUND_HEIGHT);

	/* Success */
	ret = 0;

out:
	/* Free memory */
	if (ctx)
		PNGU_ReleaseImageContext(ctx);

	return ret;
}

void Video_GetBG(int x, int y, char *img, int width, int height)
{
	int ix, iy;
	char *c, *bg;
	if (!bg_buf_rgba) return;
	c = img;
	for (iy=0; iy<height; iy++) {
		bg = (char*)bg_buf_rgba + (y+iy)*(vmode->fbWidth * 4) + x*4;
		for (ix=0; ix<width; ix++) {
			*((int*)c) = *((int*)bg);
			c += 4;
			bg += 4;
		}
	}
}

// destination: img
void Video_CompositeRGBA(int x, int y, char *img, int width, int height)
{
	int ix, iy;
	char *c, *bg, a;
	if (!bg_buf_rgba) return;
	c = img;
	for (iy=0; iy<height; iy++) {
		bg = (char*)bg_buf_rgba + (y+iy)*(vmode->fbWidth * 4) + x*4;
		for (ix=0; ix<width; ix++) {
			a = c[3];
			png_composite(c[0], c[0], a, bg[0]); // r
			png_composite(c[1], c[1], a, bg[1]); // g
			png_composite(c[2], c[2], a, bg[2]); // b
			c += 4;
			bg += 4;
		}
	}
}

void RGBA_to_YCbYCr(char *dest, char *img, int width, int height)
{
	int ix, iy, buffWidth;
	char *ip = img;
	buffWidth = width / 2;
	for (iy=0; iy<height; iy++) {
		for (ix=0; ix<width/2; ix++) {
			((PNGU_u32 *)dest)[iy*buffWidth+ix] =
			   	PNGU_RGB8_TO_YCbYCr (ip[0], ip[1], ip[2], ip[4], ip[5], ip[6]);
			ip += 8;
		}
	}
}

void Video_DrawRGBA(int x, int y, char *img, int width, int height)
{
	int ix, iy, buffWidth;
	char *ip = img;
	buffWidth = vmode->fbWidth / 2;
	for (iy=0; iy<height; iy++) {
		for (ix=0; ix<width/2; ix++) {
			((PNGU_u32 *)framebuffer)[(y+iy)*buffWidth+x/2+ix] =
			   	PNGU_RGB8_TO_YCbYCr (ip[0], ip[1], ip[2], ip[4], ip[5], ip[6]);
			ip += 8;
		}
	}
}


