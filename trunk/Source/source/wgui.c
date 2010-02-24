
// by oggzee

// useless experimental code for gui

#if 0
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "wpad.h"
#include "GRRLIB.h"
#include "gui.h"
#include "cfg.h"
#include "wgui.h"

#define WGUI_BUTTON_NORMAL 0
#define WGUI_BUTTON_HOVER 1
#define WGUI_BUTTON_PRESS 2
#define WGUI_BUTTON_DISABLED 2

#define WGUI_TYPE_BUTTON 1
#define WGUI_TYPE_CHECKBOX 2

extern unsigned char button_img[];
extern unsigned char window_img[];
extern unsigned char cb_img[];
extern unsigned char cb_val_img[];

GRRLIB_texImg tx_button;
GRRLIB_texImg tx_window;
GRRLIB_texImg tx_cb;
GRRLIB_texImg tx_cb_val;

FontColor wgui_fc = { 0xFFFFFFFF, 0x80 };

/*

Resources:

button.png
window.png

cb      (t)[t]
cb_val  x draw: (t)x
cb_img  x draw: x

cb-favorite
cb_val-favorite
cb_img-favorite

*/

static int wgui_inited = 0;

void wgui_init_button(GRRLIB_texImg *tx, u8 *img)
{
    *tx = GRRLIB_LoadTexture(img);
    GRRLIB_InitTileSet(tx, tx->w/3, tx->w/3*2, 0);
}

void wgui_init_cb(GRRLIB_texImg *tx, u8 *img)
{
    *tx = GRRLIB_LoadTexture(img);
    GRRLIB_InitTileSet(tx, tx->w/6, tx->h/2, 0);
}

void wgui_init()
{
	if (wgui_inited) return;

	wgui_init_button(&tx_button, button_img);
	wgui_init_cb(&tx_cb, cb_img);
	wgui_init_button(&tx_cb_val, cb_val_img);

    tx_window = GRRLIB_LoadTexture(window_img);
    GRRLIB_InitTileSet(&tx_window, tx_window.w/3, tx_window.h/3, 0);

	wgui_inited = 1;
}

void wgui_DrawWindowBase(int x, int y, int w, int h, u32 color)
{
	int tw = tx_window.tilew;
	int th = tx_window.tileh;
	int ww = w - tw*2;
	int hh = h - th*2;

    Vector v1[] = {
		{x+tw,    y},
		{x+tw+ww, y},
		{x+tw+ww, y+th},
		{x+tw,    y+th}
	};
    Vector v3[] = {
		{x,    y+th},
		{x+tw, y+th},
		{x+tw, y+th+hh},
		{x,    y+th+hh}
	};
    Vector v4[] = {
		{x+tw,    y+th},
		{x+tw+ww, y+th},
		{x+tw+ww, y+th+hh},
		{x+tw,    y+th+hh}
	};
    Vector v5[] = {
		{x+tw+ww, y+th},
		{x+w,     y+th},
		{x+w,     y+th+hh},
		{x+tw+ww, y+th+hh}
	};
    Vector v7[] = {
		{x+tw,    y+th+hh},
		{x+tw+ww, y+th+hh},
		{x+tw+ww, y+h},
		{x+tw,    y+h}
	};

    GRRLIB_DrawTile(x, y, tx_window, 0,1,1, color, 0);
    GRRLIB_DrawTileQuad(v1, &tx_window, color, 1);
    GRRLIB_DrawTile(x+w-tw, y, tx_window, 0,1,1, color, 2);

	GRRLIB_DrawTileQuad(v3, &tx_window, color, 3);
	GRRLIB_DrawTileQuad(v4, &tx_window, color, 4);
	GRRLIB_DrawTileQuad(v5, &tx_window, color, 5);

	GRRLIB_DrawTile(x, y+th+hh, tx_window, 0,1,1, color, 6);
	GRRLIB_DrawTileQuad(v7, &tx_window, color, 7);
	GRRLIB_DrawTile(x+tw+ww, y+th+hh, tx_window, 0,1,1, color, 8);

}

void wgui_DrawWindow(int x, int y, int w, int h, char *title)
{
	int cx = x + w / 2;
	wgui_DrawWindowBase(x, y, w, h, 0xFFFFFFFF);
	Gui_PrintAlign(cx, y+32, 0, -1, tx_font, wgui_fc, title);
}

void wgui_DrawButtonBase(GRRLIB_texImg *tx,
		int x, int y, int w, int h, u32 color, int state)
{
	int h2 = h / 2;
    Vector v1[] = {
		{x,    y},
		{x+h2, y},
		{x+h2, y+h},
		{x,    y+h}
	};
    Vector v2[] = {
		{x+h2,   y},
		{x+w-h2, y},
		{x+w-h2, y+h},
		{x+h2,   y+h}
	};
    Vector v3[] = {
		{x+w-h2, y},
		{x+w,    y},
		{x+w,    y+h},
		{x+w-h2, y+h}
	};

    GRRLIB_DrawTileQuad(v1, tx, color, state*3+0);
    GRRLIB_DrawTileQuad(v2, tx, color, state*3+1);
    GRRLIB_DrawTileQuad(v3, tx, color, state*3+2);
}

void wgui_DrawButton(GRRLIB_texImg *tx,
		int x, int y, int w, int h, int state, char *txt)
{
	int cx = x + w / 2;
	int cy = y + h / 2;

	wgui_DrawButtonBase(tx, x, y, w, h, 0xFFFFFFFF, state);

	Gui_PrintAlign(cx, cy, 0, 0, tx_font, wgui_fc, txt);
}

int wgui_HandleButton(int x, int y, int w, int h, char *txt,
		struct ir_t *wpad_ir, int wpad_button)
{
	bool over;
	int state;
	over = GRRLIB_PtInRect(x, y, w, h, wpad_ir->sx, wpad_ir->sy);
	state = WGUI_BUTTON_NORMAL;
	if (over) {
		state = WGUI_BUTTON_HOVER;
		if (wpad_button & CFG.button_confirm.mask)
			state = WGUI_BUTTON_PRESS;
	}
	wgui_DrawButton(&tx_button, x, y, w, h, state, txt);
	return state;
}

typedef struct wgui_Widget
{
	int id;
	int type;
	bool disabled;
	int x, y, w, h;
	char *text;
	int *state;
} wgui_Widget;

#define MAX_WIDGET 30

typedef struct wgui_Dialog
{
	char *title;
	int x, y, w, h;
	int num;
	wgui_Widget widget[MAX_WIDGET];
} wgui_Dialog;

enum
{
	ID_NONE = 0,
	ID_FAV,
	ID_OPT,
	ID_DEL,
	ID_BACK,
	ID_START,
};

void wgui_dialog_init(wgui_Dialog *dialog, int x, int y, int w, int h, char *title)
{
	memset(dialog, 0, sizeof(*dialog));
	dialog->x = x;
	dialog->y = y;
	dialog->w = w;
	dialog->h = h;
	dialog->title = title;
}

wgui_Widget* wgui_dialog_add(wgui_Dialog *dialog, int id, int x, int y, int w, int h)
{
	if (dialog->num >= MAX_WIDGET) return NULL;
	wgui_Widget *ww = &dialog->widget[dialog->num];
	ww->id = id;
	ww->x = x;
	ww->y = y;
	ww->w = w;
	ww->h = h;
	dialog->num++;
	return ww;
}

void wgui_dialog_add_button(wgui_Dialog *dialog, int id, int x, int y, char *text)
{
	wgui_Widget *ww = wgui_dialog_add(dialog, id, x, y, 150, 64);
	if (ww == NULL) return;
	ww->type = WGUI_TYPE_BUTTON;
	ww->text = text;
}

void wgui_dialog_add_checkbox(wgui_Dialog *dialog, int id, int x, int y, char *text, int *state)
{
	wgui_Widget *ww = wgui_dialog_add(dialog, id, x, y, 200, 64);
	if (ww == NULL) return;
	ww->type = WGUI_TYPE_CHECKBOX;
	ww->text = text;
	ww->state = state;
}

void wgui_dialog_prepare(wgui_Dialog *dialog)
{
}

bool wgui_over_widget(wgui_Widget *ww, ir_t *ir)
{
	return GRRLIB_PtInRect(ww->x, ww->y, ww->w, ww->h, ir->sx, ir->sy);
}

int wgui_handle_checkbox(wgui_Widget *ww, ir_t *ir, int buttons)
{
	bool over;
	int hoover = 0, state;
	over = wgui_over_widget(ww, ir);
	state = WGUI_BUTTON_NORMAL;
	if (over) {
		state = WGUI_BUTTON_HOVER;
		hoover = 1;
		if (buttons & CFG.button_confirm.mask) {
			*ww->state = !*ww->state;
		}
	}
	int val_w = 96;
	int w = ww->w - val_w;
	int val_x = ww->x + w;
	char *val_text = *ww->state ? "On" : "Off";
	//wgui_DrawButton(&tx_cb_name, ww->x, ww->y, w, ww->h, state, ww->text);
	//wgui_DrawButton(&tx_cb_val, val_x, ww->y, val_w, ww->h, state, val_text);
	wgui_DrawButton(&tx_cb, ww->x, ww->y, w, ww->h, hoover*2, ww->text);
	wgui_DrawButton(&tx_cb, val_x, ww->y, val_w, ww->h, hoover*2+1, val_text);
	return state;
}

int wgui_handle_widget(wgui_Widget *ww, ir_t *ir, int buttons)
{
	switch (ww->type) {
		case WGUI_TYPE_BUTTON:
			return wgui_HandleButton(ww->x, ww->y, ww->w, ww->h, ww->text,
					ir, buttons);
		case WGUI_TYPE_CHECKBOX:
			return wgui_handle_checkbox(ww, ir, buttons);
	}
	return 0;
}

int wgui_handle(wgui_Dialog *dialog, ir_t *ir, int buttons)
{
	int i, id = 0;
	int state = 0;
	wgui_DrawWindow(dialog->x, dialog->y, dialog->w, dialog->h, dialog->title);
	for (i=0; i<dialog->num; i++) {
		wgui_Widget *ww = &dialog->widget[i];
		state = wgui_handle_widget(ww, ir, buttons);
		if (state == WGUI_BUTTON_PRESS) id = ww->id;
	}
	return id;
}

void wgui_dialog_close(wgui_Dialog *dialog)
{
}

/*

   Title

 ####  Favorite: [X]
 ####
 ####  ( Options )
 ####
 ####  

 ( Back )  ( Start )

*/

void wgui_GameDialog()
{
	wgui_Dialog dialog;
	int id;
	int buttons;
	ir_t ir;
	int fav;

	wgui_dialog_init(&dialog, 20, 20, 600, 440, "Title");
	//wgui_dialog_add_checkbox(&dialog, ID_FAV, 400, 100, "Favorite:Yes/No", &fav);
	wgui_dialog_add_checkbox(&dialog, ID_FAV, 400, 100, "Favorite", &fav);
	wgui_dialog_add_button(&dialog, ID_OPT,   400, 180, "Options");
	wgui_dialog_add_button(&dialog, ID_DEL,   400, 260, "Delete");
	wgui_dialog_add_button(&dialog, ID_BACK,  150, 350, "Back");
	wgui_dialog_add_button(&dialog, ID_START, 300, 350, "Start");
	wgui_dialog_prepare(&dialog);
	
	do {
		buttons = Wpad_GetButtons();
		Wpad_getIR(WPAD_CHAN_0, &ir);
		id = wgui_handle(&dialog, &ir, buttons);
		Gui_draw_pointer(&ir);
		Gui_Render();
		if (buttons & CFG.button_cancel.mask) break;
	} while (id != ID_BACK);

	wgui_dialog_close(&dialog);
}


void wgui_test(struct ir_t *ir, int button)
{
	wgui_init();

	if (button & WPAD_BUTTON_PLUS) {
		wgui_GameDialog();
	}
	
	wgui_DrawWindowBase( 20, 20, 600, 440, 0xFFFFFFFF);
}

#endif
