#ifndef _VIDEO_H_
#define _VIDEO_H_

#include "libpng/pngu/pngu.h"

/* Prototypes */
void Con_Init(u32, u32, u32, u32);
void Con_Clear(void);
void Con_ClearLine(void);
void Con_FgColor(u32, u8);
void Con_BgColor(u32, u8);
void Con_FillRow(u32, u32, u8);

void Video_Configure(GXRModeObj *);
void Video_SetMode(void);
void Video_Clear(s32);
void Video_DrawPng(IMGCTX, PNGUPROP, u16, u16);

void FgColor(int color);
void BgColor(int color);
void DefaultColor();
bool ScreenShot(char *fname);
s32 Video_SaveBg(void *img);
void Video_DrawBg();
void Video_GetBG(int x, int y, char *img, int width, int height);
void Video_CompositeRGBA(int x, int y, char *img, int width, int height);
void Video_DrawRGBA(int x, int y, char *img, int width, int height);
void RGBA_to_YCbYCr(char *dest, char *img, int width, int height);
extern GXRModeObj* _Video_GetVMode();
void* _Video_GetFB();
void _Video_SetFB(void *fb);
void _Video_SyncFB();

void __console_flush(int retrace_min);

#endif
