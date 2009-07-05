#ifndef _GUI_H_
#define _GUI_H_

/* Prototypes */
void Gui_InitConsole(void);
void Gui_DrawBackground(void);
void Gui_DrawCover(u8 *);
s32 __Gui_GetPngDimensions(void *img, u32 *w, u32 *h);

void Gui_Init();
int  Gui_Mode();
void Gui_Close();
void Cache_Invalidate();

#endif
