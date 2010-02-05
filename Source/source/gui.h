#ifndef _GUI_H_
#define _GUI_H_

#include "wpad.h"
#include "GRRLIB.h"
#include "cfg.h"

#define BACKGROUND_WIDTH	640
#define BACKGROUND_HEIGHT	480

struct M2_texImg
{
	GRRLIB_texImg tx;
	int size;
};

extern struct M2_texImg t2_bg;
extern struct M2_texImg t2_bg_con;
extern struct M2_texImg t2_nocover;
extern struct M2_texImg t2_nocover_full;
extern struct M2_texImg t2_screenshot;

extern GRRLIB_texImg tx_pointer;
extern GRRLIB_texImg tx_hourglass;
extern GRRLIB_texImg tx_star;
extern GRRLIB_texImg tx_font;


// FLOW_Z camera
extern float cam_f;
extern float cam_dir;
extern float cam_z;
extern Vector cam_look;

extern int game_select;


/* Prototypes */
void Gui_InitConsole(void);
void Gui_Console_Alert(void);
void Gui_DrawBackground(void);	//draws the console mode background
void Gui_DrawIntro(void);
void Gui_draw_background_alpha2(u32 color1, u32 color2);
void Gui_draw_background_alpha(u32);  //draws the GX mode (grid/coverflow/etc) background
void Gui_draw_background(void);	//draws the GX mode (grid/coverflow/etc) background
void Gui_draw_pointer(ir_t *ir);
void Gui_DrawCover(u8 *discid);
int Gui_LoadCover(u8 *discid, void **p_imgData, bool);
int Gui_LoadCover_style(u8 *discid, void **p_imgData, bool, int);
s32 __Gui_DrawPng(void *img, u32 x, u32 y);
void Gui_LoadBackground(void);

GRRLIB_texImg Gui_LoadTexture_RGBA8(const unsigned char my_png[], int, int, void *dest);
GRRLIB_texImg Gui_LoadTexture_CMPR(const unsigned char my_png[], int, int, void *dest);
GRRLIB_texImg Gui_LoadTexture_fullcover(const unsigned char my_png[], int, int, int, void *dest);
GRRLIB_texImg Gui_paste_into_fullcover(void *src, int src_w, int src_h, void *dest, int dest_w, int dest_h);

s32 __Gui_GetPngDimensions(void *img, u32 *w, u32 *h);
void Gui_set_camera(ir_t *ir, int);
void Gui_Wpad_IR(int, struct ir_t *ir);
void gui_tilt_pos(Vector *pos);
void cache2_tex(struct M2_texImg *dest, GRRLIB_texImg *src);
void cache2_tex_alloc(struct M2_texImg *dest, int w, int h);
void cache2_tex_alloc_fullscreen(struct M2_texImg *dest);
void cache2_GRRLIB_tex_alloc(GRRLIB_texImg *dest, int w, int h);
void cache2_GRRLIB_tex_alloc_fullscreen(GRRLIB_texImg *dest);

void Gui_PrintAlign(int x, int y, int alignx, int aligny,
		GRRLIB_texImg font, FontColor font_color, char *str);
void Gui_PrintEx2(int x, int y, GRRLIB_texImg font,
		int color, int color_outline, int color_shadow, char *str);
void Gui_PrintEx(int x, int y, GRRLIB_texImg font, FontColor font_color, char *str);
void Gui_PrintfEx(int x, int y, GRRLIB_texImg font, FontColor font_color, char *fmt, ...);
void Gui_Printf(int x, int y, char *fmt, ...);
void Gui_Print2(int x, int y, char *str);
void Gui_Print(int x, int y, char *str);
void Gui_Print_Clock(int x, int y, FontColor font_color, time_t t);

void Gui_RenderAAPass(int aaStep);
void Gui_Render();
void Gui_Init();
int  Gui_Mode();
void Gui_Close();

#endif
