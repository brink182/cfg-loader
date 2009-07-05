#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <ogcsys.h>
#include <dirent.h>

#include "cfg.h"
#include "wpad.h"
#include "gui.h"
#include "video.h"
#include "fat.h"

char FAT_DRIVE[8] = "sd:";
char USBLOADER_PATH[200] = "sd:/usb-loader";
static char APPS_DIR[200] = "";

/* configurable fields */

int ENTRIES_PER_PAGE = 12;
int MAX_CHARACTERS   = 30;

int CONSOLE_XCOORD   = 258;
int CONSOLE_YCOORD   = 112;
int CONSOLE_WIDTH    = 354;
int CONSOLE_HEIGHT   = 304;

int CONSOLE_FG_COLOR = 15;
int CONSOLE_BG_COLOR = 0;

int COVER_XCOORD     = 24;
int COVER_YCOORD     = 104;

struct CFG CFG;

#define TITLE_MAX 65

struct ID_Title
{
	u8 id[5];
	char title[TITLE_MAX];
};

// renamed titles
int num_title = 0; //number of titles
struct ID_Title *cfg_title = NULL;

#define MAX_SAVED_GAMES 1000
int num_saved_games = 0;
struct Game_CFG cfg_game[MAX_SAVED_GAMES];

struct TextMap
{
	char *name;
	int id;
};

struct TextMap map_video[] =
{
	{ "system", CFG_VIDEO_SYS },
	{ "game",   CFG_VIDEO_GAME },
	{ "auto",   CFG_VIDEO_AUTO },
	{ "patch",  CFG_VIDEO_PATCH },
	{ "pal50",  CFG_VIDEO_PAL50 },
	{ "pal60",  CFG_VIDEO_PAL60 },
	{ "ntsc",   CFG_VIDEO_NTSC },
	{ NULL, -1 }
};

struct TextMap map_language[] =
{
	{ "console",   CFG_LANG_CONSOLE },
	{ "japanese",  CFG_LANG_JAPANESE },
	{ "english",   CFG_LANG_ENGLISH },
	{ "german",    CFG_LANG_GERMAN },
	{ "french",    CFG_LANG_FRENCH },
	{ "spanish",   CFG_LANG_SPANISH },
	{ "italian",   CFG_LANG_ITALIAN },
	{ "dutch",     CFG_LANG_DUTCH },
	{ "s.chinese", CFG_LANG_S_CHINESE },
	{ "t.chinese", CFG_LANG_T_CHINESE },
	{ "korean",    CFG_LANG_KOREAN },
	{ NULL, -1 }
};

struct TextMap map_layout[] =
{
	{ "original",  CFG_LAYOUT_ORIG },
	{ "original1", CFG_LAYOUT_ORIG },
	{ "original2", CFG_LAYOUT_ORIG_12 },
	{ "small",     CFG_LAYOUT_SMALL },
	{ "medium",    CFG_LAYOUT_MEDIUM },
	{ "large",     CFG_LAYOUT_LARGE },
	{ "large2",    CFG_LAYOUT_LARGE_2 },
	{ "large3",    CFG_LAYOUT_LARGE_3 },
	{ "ultimate1", CFG_LAYOUT_ULTIMATE1 },
	{ "ultimate2", CFG_LAYOUT_ULTIMATE2 },
	{ "ultimate3", CFG_LAYOUT_ULTIMATE3 },
	{ "kosaic",    CFG_LAYOUT_KOSAIC },
	{ NULL, -1 }
};

struct TextMap map_ios[] =
{
	{ "249",        CFG_IOS_249 },
	{ "222-yal",    CFG_IOS_222_YAL },
	{ "222-mload",  CFG_IOS_222_MLOAD },
	{ "223-yal",    CFG_IOS_223_YAL },
	{ "223-mload",  CFG_IOS_223_MLOAD },
	{ "250",        CFG_IOS_250 },
	{ NULL, -1 }
};

int CFG_IOS_MAX = sizeof(map_ios) / sizeof(struct TextMap) - 2;
int CURR_IOS_IDX = -1;

static char *cfg_name, *cfg_val;

// theme
#define MAX_THEME 100
int num_theme = 0;
int cur_theme = -1;
char theme_list[MAX_THEME][31];
char theme_path[200];


void game_set(char *name, char *val);
bool cfg_parsefile(char *fname, void (*set_func)(char*, char*));
void load_theme(char *theme);
void cfg_setup1();
void cfg_setup2();
void cfg_setup3();

void cfg_layout()
{
	// CFG_LAYOUT_ORIG (1.0 - base for most)
	ENTRIES_PER_PAGE = 6;
	MAX_CHARACTERS   = 30;
	CONSOLE_XCOORD   = 260;
	CONSOLE_YCOORD   = 115;
	CONSOLE_WIDTH    = 340;
	CONSOLE_HEIGHT   = 218;
	CONSOLE_FG_COLOR = 15;
	CONSOLE_BG_COLOR = 0;
	// orig 1.2
	COVER_XCOORD     = 24;
	COVER_YCOORD     = 104;
	COVER_WIDTH      = 160;
	COVER_HEIGHT     = 225;
	// widescreen
	CFG.W_CONSOLE_XCOORD = CONSOLE_XCOORD;
	CFG.W_CONSOLE_YCOORD = CONSOLE_YCOORD;
	CFG.W_CONSOLE_WIDTH  = CONSOLE_WIDTH;
	CFG.W_CONSOLE_HEIGHT = CONSOLE_HEIGHT;
	CFG.W_COVER_XCOORD   = COVER_XCOORD;
	CFG.W_COVER_YCOORD   = COVER_YCOORD;
	CFG.W_COVER_WIDTH  = 0; // auto
	CFG.W_COVER_HEIGHT = 0; // auto 
	switch (CFG.layout) {

		case CFG_LAYOUT_ORIG: // 1.0+
			ENTRIES_PER_PAGE = 6;
			MAX_CHARACTERS   = 30;
			CONSOLE_XCOORD   = 260;
			CONSOLE_YCOORD   = 115;
			CONSOLE_WIDTH    = 340;
			CONSOLE_HEIGHT   = 218;
			CONSOLE_FG_COLOR = 15;
			CONSOLE_BG_COLOR = 0;
			break;

		case CFG_LAYOUT_ORIG_12: // 1.2+
			ENTRIES_PER_PAGE = 12;
			MAX_CHARACTERS   = 30;
			CONSOLE_XCOORD   = 258;
			CONSOLE_YCOORD   = 112;
			CONSOLE_WIDTH    = 354;
			CONSOLE_HEIGHT   = 304;
			CONSOLE_FG_COLOR = 15;
			CONSOLE_BG_COLOR = 0;
			COVER_XCOORD     = 24;
			COVER_YCOORD     = 104;
			break;

		case CFG_LAYOUT_SMALL: // same as 1.0 + use more space
			ENTRIES_PER_PAGE = 9;
			MAX_CHARACTERS   = 38;
			break;

		case CFG_LAYOUT_MEDIUM:
			ENTRIES_PER_PAGE = 19; //17;
			MAX_CHARACTERS   = 38;
			CONSOLE_YCOORD   = 50;
			CONSOLE_HEIGHT   = 380;
			COVER_XCOORD     = 28;
			COVER_YCOORD     = 175;
			break;

		// nixx:
		case CFG_LAYOUT_LARGE:
			ENTRIES_PER_PAGE = 21;
			MAX_CHARACTERS   = 38;
			CONSOLE_YCOORD   = 40;
			CONSOLE_HEIGHT   = 402;
			COVER_XCOORD     = 28;
			COVER_YCOORD     = 175;
			break;

		// usptactical:
		case CFG_LAYOUT_LARGE_2:
			ENTRIES_PER_PAGE = 21;
			MAX_CHARACTERS   = 38;
			CONSOLE_YCOORD   = 40;
			CONSOLE_HEIGHT   = 402;
			COVER_XCOORD     = 30;
			COVER_YCOORD     = 180;
			break;

		// oggzee
		case CFG_LAYOUT_LARGE_3:
			ENTRIES_PER_PAGE = 21;
			MAX_CHARACTERS   = 38;
			CONSOLE_YCOORD   = 40;
			CONSOLE_WIDTH    = 344;
			CONSOLE_HEIGHT   = 402;
			COVER_XCOORD     = 42;
			COVER_YCOORD     = 102;
			CFG.W_CONSOLE_XCOORD = CONSOLE_XCOORD;
			CFG.W_CONSOLE_YCOORD = CONSOLE_YCOORD;
			CFG.W_CONSOLE_WIDTH  = CONSOLE_WIDTH;
			CFG.W_CONSOLE_HEIGHT = CONSOLE_HEIGHT;
			CFG.W_COVER_XCOORD   = COVER_XCOORD+14;
			CFG.W_COVER_YCOORD   = COVER_YCOORD;
			break;

		// Ultimate1: (WiiShizza) white background
		case CFG_LAYOUT_ULTIMATE1:
			ENTRIES_PER_PAGE = 12;
			MAX_CHARACTERS   = 37;
			CONSOLE_YCOORD   = 30;
			CONSOLE_HEIGHT   = 290;
			CONSOLE_FG_COLOR = 0;
			CONSOLE_BG_COLOR = 15;
			COVER_XCOORD     = 28;
			COVER_YCOORD     = 105;
			break;

		// Ultimate2: (jservs7 / hungyip84)
		case CFG_LAYOUT_ULTIMATE2:
			ENTRIES_PER_PAGE = 12;
			MAX_CHARACTERS   = 37;
			CONSOLE_YCOORD   = 30;
			CONSOLE_HEIGHT   = 290;
			CONSOLE_FG_COLOR = 15;
			CONSOLE_BG_COLOR = 0;
			COVER_XCOORD     = 28;
			COVER_YCOORD     = 105;
			break;

		// Ultimate3: (WiiShizza) white background, covers on right
		case CFG_LAYOUT_ULTIMATE3:
			ENTRIES_PER_PAGE = 12;
			MAX_CHARACTERS   = 37;
			CONSOLE_XCOORD   = 40;
			CONSOLE_YCOORD   = 71;
			CONSOLE_WIDTH    = 340;
			CONSOLE_HEIGHT   = 290;
			CONSOLE_FG_COLOR = 0;
			CONSOLE_BG_COLOR = 15;
			COVER_XCOORD     = 446;
			COVER_YCOORD     = 109;
			CFG.W_CONSOLE_XCOORD = CONSOLE_XCOORD;
			CFG.W_CONSOLE_YCOORD = CONSOLE_YCOORD;
			CFG.W_CONSOLE_WIDTH  = CONSOLE_WIDTH;
			CFG.W_CONSOLE_HEIGHT = CONSOLE_HEIGHT;
			CFG.W_COVER_XCOORD   = 482;
			CFG.W_COVER_YCOORD   = 110;
			break;

		// Kosaic (modified U3)
		case CFG_LAYOUT_KOSAIC:
			ENTRIES_PER_PAGE = 12;
			MAX_CHARACTERS   = 37;
			CONSOLE_XCOORD   = 40;
			CONSOLE_YCOORD   = 71;
			CONSOLE_WIDTH    = 344;
			CONSOLE_HEIGHT   = 290;
			CONSOLE_FG_COLOR = 15;
			CONSOLE_BG_COLOR = 0;
			COVER_XCOORD     = 446;
			COVER_YCOORD     = 109;
			CFG.W_CONSOLE_XCOORD = 34;
			CFG.W_CONSOLE_YCOORD = 71;
			CFG.W_CONSOLE_WIDTH  = CONSOLE_WIDTH;
			CFG.W_CONSOLE_HEIGHT = CONSOLE_HEIGHT;
			CFG.W_COVER_XCOORD   = 444;
			CFG.W_COVER_YCOORD   = 109;
			break;

	}
}


void set_colors(int x)
{
	switch(x) {
		case CFG_COLORS_MONO:
			CFG.color_header =
			CFG.color_inactive =
			CFG.color_footer =
			CFG.color_help =
			CFG.color_selected_bg = CONSOLE_FG_COLOR;
			CFG.color_selected_fg = CONSOLE_BG_COLOR;
			break;

		case CFG_COLORS_DARK:
			CFG.color_header = 14;
			CFG.color_selected_fg = 11;
			CFG.color_selected_bg = 12;
			CFG.color_inactive = 8;
			CFG.color_footer = 6;
			CFG.color_help = 2;
			break;

		case CFG_COLORS_BRIGHT:
			CFG.color_header = 6;
			CFG.color_selected_fg = 11;
			CFG.color_selected_bg = 12;
			CFG.color_inactive = 7;
			CFG.color_footer = 4;
			CFG.color_help = 2;
			break;
	}
}

void cfg_set_covers_path()
{
	STRCOPY(CFG.covers_path_2d, CFG.covers_path);
	snprintf(D_S(CFG.covers_path_3d), "%s/%s", CFG.covers_path, "3d");
	snprintf(D_S(CFG.covers_path_disc), "%s/%s", CFG.covers_path, "disc");
}

void cfg_set_url_style(char *val)
{
	if (strcmp(val, "standard")==0) {
		STRCOPY(CFG.cover_url_norm,
			"http://www.theotherzone.com/wii/{REGION}/{ID6}.png");
		STRCOPY(CFG.cover_url_wide,
			"http://www.theotherzone.com/wii/widescreen/{REGION}/{ID6}.png");
	} else if (strcmp(val, "3d")==0) {
		STRCOPY(CFG.cover_url_norm,
			"http://www.theotherzone.com/wii/3d/{WIDTH}/{HEIGHT}/{ID6}.png");
		STRCOPY(CFG.cover_url_wide, CFG.cover_url_norm);
	} else if (strcmp(val, "disc")==0) {
		STRCOPY(CFG.cover_url_norm,
			"http://www.theotherzone.com/wii/discs/{ID3}.png");
		STRCOPY(CFG.cover_url_wide,
			"http://www.theotherzone.com/wii/diskart/{WIDTH}/{HEIGHT}/{ID3}.png");
	}
}

void cfg_set_cover_style(char *val)
{
	if (strcmp(val, "standard")==0) {
		STRCOPY(CFG.covers_path, CFG.covers_path_2d); 
		COVER_WIDTH = 160;
		COVER_HEIGHT = 225;
	} else if (strcmp(val, "3d")==0) {
		STRCOPY(CFG.covers_path, CFG.covers_path_3d); 
		COVER_WIDTH = 160;
		COVER_HEIGHT = 225;
	} else if (strcmp(val, "disc")==0) {
		STRCOPY(CFG.covers_path, CFG.covers_path_disc); 
		COVER_WIDTH = 160;
		COVER_HEIGHT = 160;
	}
	cfg_set_url_style(val);
}


// theme controls the looks, not behaviour

void CFG_Default_Theme()
{
	*CFG.theme = 0;
	snprintf(D_S(CFG.background), "%s/%s", USBLOADER_PATH, "background.png");
	snprintf(D_S(CFG.w_background), "%s/%s", USBLOADER_PATH, "background_wide.png");
	snprintf(D_S(CFG.bg_gui), "%s/%s", USBLOADER_PATH, "bg_gui.png");
	CFG.layout   = CFG_LAYOUT_LARGE_3;
	CFG.covers   = 1;
	CFG.hide_header  = 0;
	CFG.hide_hddinfo = 0;
	CFG.hide_footer  = 0;
	CFG.console_transparent = 0;
	CFG.buttons  = CFG_BTN_OPTIONS_1;
	strcpy(CFG.cursor, ">>");
	strcpy(CFG.cursor_space, "  ");
	strcpy(CFG.menu_plus,   "[+] ");
	strcpy(CFG.menu_plus_s, "    ");

	cfg_set_cover_style("standard");

	CFG.gui_text_color = 0x000000FF; // black

	cfg_layout();
	set_colors(CFG_COLORS_DARK);
}

void CFG_Default()
{
	memset(&CFG, 0, sizeof(CFG));

	snprintf(D_S(CFG.covers_path), "%s/%s", USBLOADER_PATH, "covers");
	cfg_set_covers_path();

	CFG_Default_Theme();

	CFG.home     = CFG_HOME_REBOOT;
	CFG.debug    = 0;
	CFG.device   = CFG_DEV_ASK;
	CFG.confirm_start = 1;
	CFG.install_all_partitions = 1;
	CFG.disable_format = 0;
	CFG.disable_remove = 0;
	CFG.disable_install = 0;
	CFG.disable_options = 0;
	CFG.music = 1;
	CFG.widescreen = CFG_WIDE_AUTO;
	CFG.gui = 1;
	CFG.gui_rows = 2;
	// default game settings
	CFG.game.video    = CFG_VIDEO_AUTO;
	cfg_ios_set_idx(DEFAULT_IOS_IDX);
	// all other game settings are 0 (memset(0) above)
}


int map_get_id(struct TextMap *map, char *name, int *id_val)
{
	int i;
	for (i=0; map[i].name != NULL; i++)	{
		if (strcmp(name, map[i].name) == 0) {
			*id_val = map[i].id;
			return i;
		}
	}
	return -1;
}

char* map_get_name(struct TextMap *map, int id)
{
	int i;
	for (i=0; map[i].name != NULL; i++)	{
		if (id == map[i].id) return map[i].name;
	}
	return NULL;
}

int map_auto_i(char *name, char *name2, char *val, struct TextMap *map, int *var)
{
	if (strcmp(name, name2) != 0) return -1;
 	int i, id;
   	i = map_get_id(map, val, &id);
	if (i >= 0) *var = id;
	//printf("MAP AUTO: %s=%s : %d [%d]\n", name, val, id, i); sleep(1);
	return i;
}

bool map_auto(char *name, char *name2, char *val, struct TextMap *map, int *var)
{
	int i = map_auto_i(name, name2, val, map, var);
	return i >= 0;
}


bool cfg_map_auto(char *name, struct TextMap *map, int *var)
{
	return map_auto(name, cfg_name, cfg_val, map, var);
}

bool cfg_map(char *name, char *val, int *var, int id)
{
	if (strcmp(name, cfg_name)==0 && strcmp(val, cfg_val)==0) {
		*var = id;
		return true;
	}
	return false;
}

// ignore value case:
void cfg_mapi(char *name, char *val, int *var, int id)
{
	if (strcmp(name, cfg_name)==0 && stricmp(val, cfg_val)==0) *var = id;
}

bool cfg_bool(char *name, int *var)
{
	if (cfg_map(name, "0", var, 0)) return true;
	if (cfg_map(name, "1", var, 1)) return true;
	return false;
}

bool cfg_int_hex(char *name, int *var)
{
	if (strcmp(name, cfg_name)==0) {
		int i;
		if (sscanf(cfg_val, "%x", &i) == 1) {
			*var = i;
			return true;
		}
	}
	return false;
}

bool cfg_int_max(char *name, int *var, int max)
{
	if (strcmp(name, cfg_name)==0) {
		int i;
		if (sscanf(cfg_val, "%d", &i) == 1) {
			if (i >= 0 && i <= max) {
				*var = i;
				return true;
			}
		}
	}
	return false;
}

char *cfg_get_title(u8 *id)
{
	int i;
	for (i=0; i<num_title; i++) {
		if (memcmp(id, cfg_title[i].id, 4) == 0) {
			return cfg_title[i].title;
		}
	}
	return NULL;
}

char *get_title(struct discHdr *header)
{
	char *title = cfg_get_title(header->id);
	if (title) return title;
	return header->title;
}

void title_set(char *id, char *title)
{
	char *idt = cfg_get_title((u8*)id);
	if (idt) {
		// replace
		strcopy(idt, title, TITLE_MAX);
	} else {
		cfg_title = realloc(cfg_title, (num_title+1) * sizeof(struct ID_Title));
		if (!cfg_title) {
			// error
			num_title = 0;
			return;
		}
		// add
		memcpy(cfg_title[num_title].id, id, 4);
		cfg_title[num_title].id[4] = 0;
		strcopy(cfg_title[num_title].title, title, TITLE_MAX);
		num_title++;
	}
}

// trim leading and trailing whitespace
// copy at max n or at max size-1
char* trim_n_copy(char *dest, char *src, int n, int size)
{
	int len;
	// trim leading white space
	while (isspace(*src)) { src++; n--; }
	len = strlen(src);
	if (len > n) len = n;
	// trim trailing white space
	while (len > 0 && isspace(src[len-1])) len--;
	if (len >= size) len = size-1;
	strncpy(dest, src, len);
	dest[len] = 0;
	//printf("trim_copy: '%s' %d\n", dest, len); //sleep(1);
	return dest;
}

char* trimcopy(char *dest, char *src, int size)
{
	return trim_n_copy(dest, src, size, size);
}

// returns ptr to next token
char* split_token(char *dest, char *src, char delim, int size)
{
	char *next;
	next = strchr(src, delim);
	if (next) {
		trim_n_copy(dest, src, next-src, size);
		next++;
	} else {
		trimcopy(dest, src, size);
	}
	return next;
}

// split line to part1 delimiter part2 
bool trimsplit(char *line, char *part1, char *part2, char delim, int size)
{
	char *eq = strchr(line, delim);
	if (!eq) return false;
	trim_n_copy(part1, line, eq-line, size);
	trimcopy(part2, eq+1, size);
	return true;
}

void unquote(char *dest, char *str, int size)
{
	// unquote "xx"
	if (str[0] == '"' && str[strlen(str)-1] == '"') {
		int len = strlen(str) - 2;
		if (len > size-1) len = size-1;
		if (len < 0) len = 0;
		strcopy(dest, str+1, len + 1);
	} else {
		strcopy(dest, str, size);
	}
}

#define COPY_PATH(D,S) copy_path(D,S,sizeof(D))

void copy_path(char *dest, char *val, int size)
{
	if (strchr(val, ':')) {
		// absolute path with drive (sd:/images/...)
		strcopy(dest, val, size);
	} else if (val[0] == '/') {
		// absolute path without drive (/images/...)
		snprintf(dest, size, "%s%s", FAT_DRIVE, val);
	} else {
		snprintf(dest, size, "%s/%s", USBLOADER_PATH, val);
	}
}

void copy_theme_path(char *dest, char *val, int size)
{
	// check if it's an absolute path (contains : as in sd:/...)
	if (strchr(val, ':')) {
		// absolute path with drive (sd:/images/...)
		strcopy(dest, val, size);
	} else if (val[0] == '/') {
		// absolute path without drive (/images/...)
		snprintf(dest, size, "%s%s", FAT_DRIVE, val);
	} else if (*theme_path) {
		struct stat st;
		snprintf(dest, size, "%s/%s", theme_path, val);
		if (stat(dest, &st) == 0) return;
	}
	snprintf(dest, size, "%s/%s", USBLOADER_PATH, val);
}

void cfg_id_list(char *name, char list[][8], int *num_list, int max_list)
{
	if (strcmp(name, cfg_name)==0) {
		char id[8], *next = cfg_val;
		while (next) {
			*id = 0;
			next = split_token(id, next, ',', sizeof(id));
			if (strcmp(id, "0")==0) {
				// reset list
				*num_list = 0;
			}
			if (strlen(id) == 4) {
				if (*num_list >= max_list) continue;
				// add id to hide list.
				strcopy(list[*num_list], id, 8);
				(*num_list)++;
			}
		}
	}
}

// theme options

void theme_set(char *name, char *val)
{
	cfg_name = name;
	cfg_val = val;

	if (cfg_map_auto("layout", map_layout, &CFG.layout)) {
		cfg_layout();
	}

	cfg_bool("covers",  &CFG.covers);
	cfg_bool("hide_header",  &CFG.hide_header);
	cfg_bool("hide_hddinfo", &CFG.hide_hddinfo);
	cfg_bool("hide_footer",  &CFG.hide_footer);

	cfg_bool("console_transparent", &CFG.console_transparent);

	cfg_map("buttons", "original",   &CFG.buttons, CFG_BTN_ORIGINAL);
	cfg_map("buttons", "options",    &CFG.buttons, CFG_BTN_OPTIONS_1);
	cfg_map("buttons", "options_1",  &CFG.buttons, CFG_BTN_OPTIONS_1);
	//cfg_map("buttons", "ultimate",   &CFG.buttons, CFG_BTN_ULTIMATE);// obsolete
	cfg_map("buttons", "options_B",  &CFG.buttons, CFG_BTN_OPTIONS_B);

	cfg_int_max("color_header",      &CFG.color_header, 15);
	cfg_int_max("color_selected_fg", &CFG.color_selected_fg, 15);
	cfg_int_max("color_selected_bg", &CFG.color_selected_bg, 15);
	cfg_int_max("color_inactive",    &CFG.color_inactive, 15);
	cfg_int_max("color_footer",      &CFG.color_footer, 15);
	cfg_int_max("color_help",        &CFG.color_help, 15);

	int colors = 0;
	cfg_map("colors", "mono",   &colors, CFG_COLORS_MONO);
	cfg_map("colors", "dark",   &colors, CFG_COLORS_DARK);
	cfg_map("colors", "light",  &colors, CFG_COLORS_BRIGHT);
	cfg_map("colors", "bright", &colors, CFG_COLORS_BRIGHT);
	if (colors) set_colors(colors);

	// is simple is specified in theme it only affects the looks,
	// does not lock down admin modes
	int simpl;
	if (cfg_bool("simple",  &simpl)) {
		if (simpl == 1) {
			CFG.hide_hddinfo = 1;
			CFG.hide_footer = 1;
		} else { // simple == 0
			CFG.hide_hddinfo = 0;
			CFG.hide_footer = 0;
		}
	}

	if (strcmp(name, "background")==0) {
		if (strcmp(val, "0")==0) {
			*CFG.background = 0;
		} else {
			copy_theme_path(CFG.background, val, sizeof(CFG.background));
		}
	}

	if (strcmp(name, "wbackground")==0) {
		if (strcmp(val, "0")==0) {
			*CFG.w_background = 0;
		} else {
			copy_theme_path(CFG.w_background, val, sizeof(CFG.w_background));
		}
	}

	if (strcmp(name, "console_coords")==0) {
		int x,y,w,h;
		if (sscanf(val, "%d,%d,%d,%d", &x, &y, &w, &h) == 4) {
			// round x coords to even values
			CONSOLE_XCOORD = x / 2 * 2;
			CONSOLE_YCOORD = y;
			CONSOLE_WIDTH  = w / 2 * 2;
			CONSOLE_HEIGHT = h;
		}
	}
	if (strcmp(name, "wconsole_coords")==0) {
		int x,y,w,h;
		if (sscanf(val, "%d,%d,%d,%d", &x, &y, &w, &h) == 4) {
			// round x coords to even values
			CFG.W_CONSOLE_XCOORD = x / 2 * 2;
			CFG.W_CONSOLE_YCOORD = y;
			CFG.W_CONSOLE_WIDTH  = w / 2 * 2;
			CFG.W_CONSOLE_HEIGHT = h;
		}
	}
	if (strcmp(name, "console_color")==0) {
		int fg,bg;
		if (sscanf(val, "%d,%d", &fg, &bg) == 2) {
			CONSOLE_FG_COLOR = fg;
			CONSOLE_BG_COLOR = bg;
		}
	}
	if (strcmp(name, "console_entries")==0) {
		int n;
		if (sscanf(val, "%d", &n) == 1) {
			ENTRIES_PER_PAGE = n;
		}
	}
	if (strcmp(name, "covers_coords")==0) {
		int x,y;
		if (sscanf(val, "%d,%d", &x, &y) == 2) {
			COVER_XCOORD = x / 2 * 2;
			COVER_YCOORD = y;
		}
	}
	if (strcmp(name, "wcovers_coords")==0) {
		int x,y;
		if (sscanf(val, "%d,%d", &x, &y) == 2) {
			CFG.W_COVER_XCOORD = x / 2 * 2;
			CFG.W_COVER_YCOORD = y;
		}
	}
	if (strcmp(name, "covers_size")==0) {
		int w,h;
		if (sscanf(val, "%d,%d", &w, &h) == 2) {
			COVER_WIDTH = w / 2 * 2;
			COVER_HEIGHT = h;
		}
	}
	if (strcmp(name, "wcovers_size")==0) {
		int w,h;
		if (sscanf(val, "%d,%d", &w, &h) == 2) {
			CFG.W_COVER_WIDTH = w / 2 * 2;
			CFG.W_COVER_HEIGHT = h;
		}
	}
	if (strcmp(name, "cover_style")==0) {
		cfg_set_cover_style(val);
	}
	if (strcmp(name, "cover_url")==0) {
		strcopy(CFG.cover_url_norm, val, sizeof(CFG.cover_url_norm));
		strcopy(CFG.cover_url_wide, val, sizeof(CFG.cover_url_wide));
	}
	if (strcmp(name, "cover_url_norm")==0) {
		strcopy(CFG.cover_url_norm, val, sizeof(CFG.cover_url_norm));
	}
	if (strcmp(name, "cover_url_wide")==0) {
		strcopy(CFG.cover_url_wide, val, sizeof(CFG.cover_url_wide));
	}
	if (strcmp(name, "cursor")==0) {
		unquote(CFG.cursor, val, 2+1);
		int i;
		for (i=0; i<strlen(CFG.cursor); i++) CFG.cursor_space[i] = ' ';
		CFG.cursor_space[i] = 0;
	}
	if (strcmp(name, "menu_plus")==0) {
		unquote(CFG.menu_plus, val, 4+1);
		int i;
		for (i=0; i<strlen(CFG.menu_plus); i++) CFG.menu_plus_s[i] = ' ';
		CFG.menu_plus_s[i] = 0;
	}
	cfg_map("gui_text_color", "black", &CFG.gui_text_color, 0x000000FF);
	cfg_map("gui_text_color", "white", &CFG.gui_text_color, 0xFFFFFFFF);
	cfg_int_hex("gui_text_color", &CFG.gui_text_color);
}

// global options

char *ios_str(int idx)
{
	return map_ios[idx].name;
}

void cfg_ios_set_idx(int ios_idx)
{
	CFG.ios = 249;
	CFG.ios_yal = 0;
	CFG.ios_mload = 0;

	switch (ios_idx) {
		case CFG_IOS_249:
			break;
		case CFG_IOS_250:
			CFG.ios = 250;
			break;
		case CFG_IOS_222_YAL:
			CFG.ios = 222;
			CFG.ios_yal = 1;
			break;
		case CFG_IOS_222_MLOAD:
			CFG.ios = 222;
			CFG.ios_yal = 1;
			CFG.ios_mload = 1;
			break;
		case CFG_IOS_223_YAL:
			CFG.ios = 223;
			CFG.ios_yal = 1;
			break;
		case CFG_IOS_223_MLOAD:
			CFG.ios = 223;
			CFG.ios_yal = 1;
			CFG.ios_mload = 1;
			break;
		default:
			ios_idx = CFG_IOS_249;
 	}

	CFG.game.ios_idx = ios_idx;
}

bool cfg_ios_idx(char *name, char *val, int *idx)
{
	int i, id;
	i = map_auto_i("ios", name, val, map_ios, &id);
	if (i < 0) return false;
	if (i > CFG_IOS_MAX) return false;
	if (i != id) return false; // safety check for correct defines
	*idx = i;
	return true;
}

void cfg_ios(char *name, char *val)
{
	int i, ios;
	if (!cfg_ios_idx(name, val, &i)) return;
	if (sscanf(val, "%d", &ios) != 1) return;

	CFG.ios = ios;
	CFG.game.ios_idx = i;
	CFG.ios_yal = 0;
	CFG.ios_mload = 0;

	if (strstr(val, "-yal")) {
		CFG.ios_yal = 1;
	}

	if (strstr(val, "-mload")) {
		CFG.ios_yal = 1;
		CFG.ios_mload = 1;
	}

}

void cfg_set_game(char *name, char *val, struct Game_CFG *game_cfg)
{
	cfg_name = name;
	cfg_val = val;

	cfg_map_auto("video", map_video, &game_cfg->video);
	if (strcmp("video", name) == 0 && strcmp("vidtv", val) == 0)
	{
		game_cfg->video = CFG_VIDEO_AUTO;
		game_cfg->vidtv = 1;
	}
	cfg_map_auto("language", map_language, &game_cfg->language);
	cfg_bool("vidtv", &game_cfg->vidtv);
	cfg_bool("country_patch", &game_cfg->country_patch);
	cfg_bool("fix_002", &game_cfg->fix_002);
	cfg_ios_idx(name, val, &game_cfg->ios_idx);
	cfg_bool("block_ios_reload", &game_cfg->block_ios_reload);
	cfg_bool("alt_dol", &game_cfg->alt_dol);
	cfg_bool("ocarina", &game_cfg->ocarina);
}


void cfg_set(char *name, char *val)
{
	cfg_name = name;
	cfg_val = val;

	theme_set(name, val);

	cfg_bool("debug",   &CFG.debug);
	cfg_bool("confirm_start", &CFG.confirm_start);
	cfg_bool("confirm_ocarina", &CFG.confirm_ocarina);
	cfg_bool("disable_format", &CFG.disable_format); 
	cfg_bool("disable_remove", &CFG.disable_remove); 
	cfg_bool("disable_install", &CFG.disable_install);
	cfg_bool("disable_options", &CFG.disable_options);

	cfg_map("home", "exit",   &CFG.home, CFG_HOME_EXIT);
	cfg_map("home", "reboot", &CFG.home, CFG_HOME_REBOOT);
	cfg_map("home", "screenshot", &CFG.home, CFG_HOME_SCRSHOT);

	cfg_map("device", "ask",  &CFG.device, CFG_DEV_ASK);
	cfg_map("device", "usb",  &CFG.device, CFG_DEV_USB);
	cfg_map("device", "sdhc", &CFG.device, CFG_DEV_SDHC);

	cfg_int_max("cursor_jump", &CFG.cursor_jump, 50);

	cfg_bool("widescreen", &CFG.widescreen);
	cfg_map("widescreen", "auto", &CFG.widescreen, CFG_WIDE_AUTO);

	cfg_set_game(name, val, &CFG.game);

	cfg_ios(name, val);

	if (strcmp(name, "covers_path")==0) {
		COPY_PATH(CFG.covers_path, val);
		cfg_set_covers_path();
	}
	if (strcmp(name, "covers_path_2d")==0) {
		COPY_PATH(CFG.covers_path_2d, val);
	}
	if (strcmp(name, "covers_path_3d")==0) {
		COPY_PATH(CFG.covers_path_3d, val);
	}
	if (strcmp(name, "covers_path_disc")==0) {
		COPY_PATH(CFG.covers_path_disc, val);
	}

	
	cfg_bool("gui", &CFG.gui);
	cfg_map("gui", "start", &CFG.gui, CFG_GUI_START);

	cfg_map("gui_transition", "scroll", &CFG.gui_transit, 0);
	cfg_map("gui_transition", "fade",   &CFG.gui_transit, 1);

	cfg_map("gui_style", "grid", &CFG.gui_style, 0);
	cfg_map("gui_style", "flow", &CFG.gui_style, 1);
	cfg_map("gui_style", "flow-z", &CFG.gui_style, 2);

	int rows = 0;
	cfg_int_max("gui_rows", &rows, 4);
	if (rows > 0) CFG.gui_rows = rows;

	// simple changes dependant options
	int simpl;
	if (cfg_bool("simple",  &simpl)) {
		if (simpl == 1) {
			CFG.confirm_start = 0;
			CFG.hide_hddinfo = 1;
			CFG.hide_footer = 1;
			CFG.disable_remove = 1;
			CFG.disable_install = 1;
			CFG.disable_options = 1;
			CFG.disable_format = 1;
		} else { // simple == 0
			CFG.confirm_start = 1;
			CFG.hide_hddinfo = 0;
			CFG.hide_footer = 0;
			CFG.disable_remove = 0;
			CFG.disable_install = 0;
			CFG.disable_options = 0;
			CFG.disable_format = 0;
		}
	}
	cfg_map("install_partitions", "all", &CFG.install_all_partitions, 1);
	cfg_map("install_partitions", "only_game", &CFG.install_all_partitions, 0);

	cfg_id_list("hide_game", CFG.hide_game, &CFG.num_hide_game, MAX_HIDE_GAME);
	cfg_id_list("pref_game", CFG.pref_game, &CFG.num_pref_game, MAX_PREF_GAME);
	cfg_id_list("favorite_game", CFG.favorite_game,
			&CFG.num_favorite_game, MAX_FAVORITE_GAME);

	if (strcmp(name, "music")==0) {
		if (cfg_bool("music", &CFG.music)) {
			*CFG.music_file = 0;
		} else {
			COPY_PATH(CFG.music_file, val);
		}
	}
	
	if (strncmp(name, "title:", 6)==0) {
		char id[8];
		trimcopy(id, name+6, sizeof(id)); 
		title_set(id, val);
	}
	if (strncmp(name, "game:", 5)==0) {
		game_set(name, val);
	}

	if (strcmp(name, "theme")==0) {
		// theme must be last, because it changes cfg_name, cfg_val
		load_theme(val);
		return;
	}
}

int find_in_hdr_list(char *id, struct discHdr *list, int num)
{
	int i;
	for (i=0; i<num; i++) {
		if (memcmp(id, list[i].id, 4) == 0) return i;
	}
	return -1;
}

int find_in_game_list(u8 *id, char list[][8], int num)
{
	int i;
	for (i=0; i<num; i++) {
		if (memcmp(id, list[i], 4) == 0) return i;
	}
	return -1;
}

void remove_from_list(int i, char list[][8], int *num)
{
	memset( list[i], 0, sizeof(list[i]) );
	if (i < *num - 1) {
		memmove( &list[i], &list[i+1], (*num - i - 1) * sizeof(list[0]) );
	}
	*num -= 1;
}

bool add_to_list(u8 *id, char list[][8], int *num, int maxn)
{
	if (*num >= maxn) return false;
	// add
	memset(list[*num], 0, sizeof(list[0]));
	memcpy(list[*num], id, 4);
	*num += 1;
	return true;
}

bool is_in_game_list(u8 *id, char list[][8], int num)
{
	int i = find_in_game_list(id, list, num);
	return (i >= 0);
}

bool is_in_hide_list(struct discHdr *game)
{
	return is_in_game_list(game->id, CFG.hide_game, CFG.num_hide_game);
}

int CFG_hide_games(struct discHdr *list, int cnt)
{
	int i;
	for (i=0; i<cnt;) {
		if (is_in_hide_list(list+i)) {
			// move remaining entries over
			memcpy(list+i, list+i+1, (cnt-i-1) * sizeof(struct discHdr));
			cnt--;
		} else {
			i++;
		}
	}
	return cnt;
}

void CFG_sort_pref(struct discHdr *list, int cnt)
{
	int i, j, tmp_cnt = 0;
	struct discHdr *tmp_list;
	tmp_list = malloc(sizeof(struct discHdr)*cnt);
	if (tmp_list == NULL) return;
	for (i=0; i<CFG.num_pref_game; i++) {
		j = find_in_hdr_list(CFG.pref_game[i], list, cnt);
		if (j < 0) continue;
		// move entry to tmp
		memcpy(&tmp_list[tmp_cnt], &list[j], sizeof(struct discHdr));
		tmp_cnt++;
		// move remaining entries over
		memcpy(list+j, list+j+1, (cnt-j-1) * sizeof(struct discHdr));
		cnt--;
	}
	// append remaining list to tmp
	memcpy(&tmp_list[tmp_cnt], list, sizeof(struct discHdr)*cnt);
	// move everything back to list
	memcpy(list, tmp_list, sizeof(struct discHdr)*(cnt+tmp_cnt));
	free(tmp_list);
}

bool is_favorite(u8 *id)
{
	return is_in_game_list(id, CFG.favorite_game,
			CFG.num_favorite_game);
}

bool set_favorite(u8 *id, bool fav)
{
	int i;
	bool ret;
	i = find_in_game_list(id, CFG.favorite_game, CFG.num_favorite_game);
	if (i >= 0) {
		// alerady on the list
		if (fav) return true;
		// remove
		remove_from_list(i, CFG.favorite_game, &CFG.num_favorite_game);
		return true;
	}
	// not on list
	if (!fav) return true;
	// add
	ret = add_to_list(id, CFG.favorite_game, &CFG.num_favorite_game, MAX_FAVORITE_GAME);
	if (!ret) return ret;
	return CFG_Save_Settings();
}

int CFG_filter_favorite(struct discHdr *list, int cnt)
{
	int i;
	//printf("f filter %p %d\n", list, cnt); sleep(2);
	for (i=0; i<cnt;) {
		//printf("%d %s %d\n", i, list[i].id, is_favorite(list[i].id));
		if (!is_favorite(list[i].id)) {
			// move remaining entries over
			memcpy(list+i, list+i+1, (cnt-i-1) * sizeof(struct discHdr));
			cnt--;
		} else {
			i++;
		}
	}
	//printf("e filter %p %d\n", list, cnt); sleep(5);
	return cnt;
}


// split line to name = val and trim whitespace
void cfg_parseline(char *line, void (*set_func)(char*, char*))
{
	// split name = value
	char name[200], val[400];
	if (!trimsplit(line, name, val, '=', sizeof(name))) return;
	//printf("CFG: '%s=%s'\n", name, val); //sleep(1);
	set_func(name, val);
}

bool cfg_parsefile(char *fname, void (*set_func)(char*, char*))
{
	FILE *f;
	char line[500];

	//printf("opening(%s)\n", fname); sleep(3);
	f = fopen(fname, "rb");
	if (!f) {
		//printf("error opening(%s)\n", fname); sleep(3);
		return false;
	}

	while (fgets(line, sizeof(line), f)) {
		// lines starting with # are comments
		if (line[0] == '#') continue;
		cfg_parseline(line, set_func);
	}
	fclose(f);
	return true;
}

void cfg_parsearg(int argc, char **argv)
{
	int i;
	char *eq;
	char pathname[200];
	for (i=1; i<argc; i++) {
		//printf("arg[%d]: %s\n", i, argv[i]);
		eq = strchr(argv[i], '=');
		if (eq) {
			cfg_parseline(argv[i], &cfg_set);
		} else {
			snprintf(pathname, sizeof(pathname), "%s/%s", USBLOADER_PATH, argv[i]);
			cfg_parsefile(pathname, &cfg_set);
		}
	}
}


// PER-GAME SETTINGS


// return existing or new
struct Game_CFG* cfg_get_game(u8 *id)
{
	struct Game_CFG *game = CFG_get_game_opt(id);
	if (game) return game;
	if (num_saved_games >= MAX_SAVED_GAMES) return NULL;
	game = &cfg_game[num_saved_games];
	num_saved_games++;
	return game;
}

// current options to game
void cfg_set_game_opt(struct Game_CFG *game, u8 *id)
{
	*game = CFG.game;
	strncpy((char*)game->id, (char*)id, 6);
	game->id[6] = 0;
}

// free game opt slot
void cfg_free_game(struct Game_CFG* game)
{
	int i;
	if (num_saved_games == 0 || !game) return;
	// move entries down
	num_saved_games--;
	for (i=game-cfg_game; i<num_saved_games; i++) {
		cfg_game[i] = cfg_game[i+1];
	}
	memset(&cfg_game[num_saved_games], 0, sizeof(struct Game_CFG));
}

void game_set(char *name, char *val)
{
	// sample line:
	// game:RTNP41 = video:game; language:english; ocarina:0;
	// game:RYWP01 = video:patch; language:console; ocarina:1;
	//printf("GAME: '%s=%s'\n", name, val);
	u8 id[8];
	struct Game_CFG *game;
	if (strncmp(name, "game:", 5) != 0) return;
	trimcopy((char*)id, name+5, sizeof(id)); 
	game = cfg_get_game(id);
	// set id and current options as default
	cfg_set_game_opt(game, id);
	//printf("GAME(%s) '%s'\n", id, val); sleep(1);

	// parse val
	// first split options by ;
	char opt[100], *p;
	p = val;

	while(p) {
		p = split_token(opt, p, ';', sizeof(opt));
		//printf("GAME(%s) (%s)\n", id, opt); sleep(1);
		// parse opt 'language:english'
		char opt_name[100], opt_val[100];
		if (trimsplit(opt, opt_name, opt_val, ':', sizeof(opt_name))){
			//printf("GAME(%s) (%s=%s)\n", id, opt_name, opt_val); sleep(1);
			cfg_set_game(opt_name, opt_val, game);
		}
	}
}

void settings_set(char *name, char *val)
{
	cfg_name = name;
	cfg_val = val;
	cfg_id_list("favorite_game", CFG.favorite_game,
			&CFG.num_favorite_game, MAX_FAVORITE_GAME);
	game_set(name, val);
}


bool CFG_Load_Settings()
{
	char pathname[200];
	snprintf(pathname, sizeof(pathname), "%s/%s", USBLOADER_PATH, "settings.cfg");
	CFG.num_favorite_game = 0;
	return cfg_parsefile(pathname, &settings_set);
}

bool CFG_Save_Settings()
{
	FILE *f;
	int i;
	struct stat st;
	char pathname[200];

	snprintf(D_S(pathname), "%s/", FAT_DRIVE);
	if (stat(pathname, &st) == -1) {
		printf("ERROR: %s inaccesible!\n", pathname);
		sleep(1);
		return false;
	}
	if (stat(USBLOADER_PATH, &st) == -1) {
		//mkdir("sd:/usb-loader", 0777);
		printf("ERROR: %s inaccesible!\n", USBLOADER_PATH);
		sleep(1);
		return false;
	}

	snprintf(D_S(pathname), "%s/%s", USBLOADER_PATH, "settings.cfg");
	f = fopen(pathname, "wb");
	if (!f) {
		printf("Error saving %s\n", pathname);
		sleep(1);
		return false;
	}
	fprintf(f, "# CFG USB Loader settings file\n");
	fprintf(f, "# Note: This file is automatically generated\n");

	fprintf(f, "# Favorite Games: %d\n", CFG.num_favorite_game);
	for (i=0; i<CFG.num_favorite_game; i++) {
		fprintf(f, "favorite_game = %.4s\n", CFG.favorite_game[i]);
	}
	fprintf(f, "# Game Options: %d\n", num_saved_games);
	for (i=0; i<num_saved_games; i++) {
		char *s;
		fprintf(f, "game:%s = ", cfg_game[i].id);
		#define SAVE_STR(N, S) \
			if (S) fprintf(f, "%s:%s; ", N, S)
		#define SAVE_BOOL(N) \
			fprintf(f, "%s:%d; ", #N, cfg_game[i].N)
		s = map_get_name(map_video, cfg_game[i].video);
		SAVE_STR("video", s);
		s = map_get_name(map_language, cfg_game[i].language);
		SAVE_STR("language", s);
		s = ios_str(cfg_game[i].ios_idx);
		SAVE_STR("ios", s);
		SAVE_BOOL(vidtv);
		SAVE_BOOL(country_patch);
		SAVE_BOOL(fix_002);
		SAVE_BOOL(block_ios_reload);
		SAVE_BOOL(alt_dol);
		SAVE_BOOL(ocarina);
		fprintf(f, "\n");
	}
	fprintf(f, "# END\n");
	fclose(f);
	return true;
}

struct Game_CFG* CFG_get_game_opt(u8 *id)
{
	int i;
	for (i=0; i<num_saved_games; i++) {
		if (memcmp(id, cfg_game[i].id, 6) == 0) {
			return &cfg_game[i];
		}
	}
	return NULL;
}

bool CFG_save_game_opt(u8 *id)
{
	struct Game_CFG *game = cfg_get_game(id);
	if (!game) return false;
	cfg_set_game_opt(game, id);
	if (CFG_Save_Settings()) return true;
	// saving failed, forget the just created entry
	cfg_free_game(game);
	return false;
}

bool CFG_forget_game_opt(u8 *id)
{
	struct Game_CFG *game = CFG_get_game_opt(id);
	if (!game) return true;
	cfg_free_game(game);
	return CFG_Save_Settings();
}


// theme

void get_theme_list()
{
	DIR *dir;
	struct dirent *dent;
	struct stat st;
	char theme_base[200] = "";
	char theme_dir[200] = "";
	char theme_file[200] = "";
	int i;

	snprintf(theme_base, sizeof(theme_base), "%s/themes", USBLOADER_PATH);
	dir = opendir(theme_base);
	if (!dir) return;

	while ((dent = readdir(dir)) != NULL) {
		if (dent->d_name[0] == '.') continue;
		snprintf(theme_dir, sizeof(theme_dir), "%s/%s", theme_base, dent->d_name);
		if (stat(theme_dir, &st) != 0) continue;
		if (!S_ISDIR(st.st_mode)) continue;
		snprintf(theme_file, sizeof(theme_file), "%s/theme.txt", theme_dir);
		if (stat(theme_file, &st) != 0) continue;
		if(S_ISREG(st.st_mode)) {
			// theme found
			//printf("Theme: %s\n", dent->d_name);
			if (num_theme >= MAX_THEME) break;
			strcopy(theme_list[num_theme], dent->d_name, sizeof(theme_list[num_theme]));
			num_theme++;
		}
	}
	closedir(dir);
	if (!num_theme) return;
	qsort(theme_list, num_theme, sizeof(*theme_list),
			(int(*)(const void*, const void*))stricmp);
	// define current
	if (!*CFG.theme) return;
	for (i=0; i<num_theme; i++) {
		if (stricmp(CFG.theme, theme_list[i]) == 0) {
			cur_theme = i;
			strcopy(CFG.theme, theme_list[i], sizeof(CFG.theme));
			break;
		}
	}
}

void load_theme(char *theme)
{
	char pathname[200];
	struct stat st;

	snprintf(theme_path, sizeof(theme_path), "%s/themes/%s", USBLOADER_PATH, theme);
	snprintf(pathname, sizeof(pathname), "%s/theme.txt", theme_path);
	if (stat(pathname, &st) == 0) {
		CFG_Default_Theme();
		STRCOPY(CFG.theme, theme);
		copy_theme_path(CFG.background, "background.png", sizeof(CFG.background));
		copy_theme_path(CFG.w_background, "background_wide.png", sizeof(CFG.w_background));
		copy_theme_path(CFG.bg_gui, "bg_gui.png", sizeof(CFG.bg_gui));
		cfg_parsefile(pathname, &theme_set);
	}
	*theme_path = 0;
}

void CFG_switch_theme(int theme_i)
{
	if (num_theme <= 0) return;
	if (cur_theme == -1) theme_i = 0;
	if (theme_i < 0) theme_i = num_theme - 1;
	if (theme_i >= num_theme) theme_i = 0;
	load_theme(theme_list[theme_i]);
	cur_theme = theme_i;
	__console_flush(0);
	cfg_setup2();
	Gui_DrawBackground();
	Gui_InitConsole();
	cfg_setup3();
}

void cfg_debug(int argc, char **argv)
{
	if (CFG.debug) {
		printf("base_path: %s\n", USBLOADER_PATH);
		printf("apps_path: %s\n", APPS_DIR);
		printf("bg_path: %s\n", *CFG.background ? CFG.background : "builtin");
		printf("covers_path: %s\n", CFG.covers_path);
		printf("theme: %s \n", CFG.theme);
		printf("covers: %d ", CFG.covers);
		printf("w: %d ", COVER_WIDTH);
		printf("h: %d ", COVER_HEIGHT);
		printf("layout: %d ", CFG.layout);
		printf("video: %d ", CFG.game.video);
		printf("home: %d ", CFG.home);
		printf("ocarina: %d ", CFG.game.ocarina);
		printf("titles: %d ", num_title);
		printf("maxc: %d ", MAX_CHARACTERS);
		printf("ent: %d ", ENTRIES_PER_PAGE);
		printf("music: %d ", CFG.music);
		printf("gui: %d ", CFG.gui);
		printf("\n");
		int i;
		for (i=0; i<argc; i++) {
			printf("arg[%d]: %s ", i, argv[i]);
		}
		sleep(1);
		printf("\n    Press any button to continue...");
		Wpad_WaitButtons();
		printf("\n\n");
	}
}

void chdir_app(char *arg)
{
	char dir[100], *pos1, *pos2;
	struct stat st;
	if (arg == NULL) goto default_dir;
	// replace fat: with sd:
	pos1 = strchr(arg, ':');
	// trim file name
	pos2 = strrchr(arg, '/');
	if (pos1 && pos2) {
		pos1++;
		//pos2++; don't want trailing /
		strncpy(dir, pos1, pos2 - pos1);
		dir[pos2 - pos1] = 0;
		if (*dir == 0) {
			// started with wiiload, use default apps_dir
			goto default_dir;
		}
		strcpy(APPS_DIR, FAT_DRIVE);
		strcat(APPS_DIR, dir);
	} else {
		// no path in arg, try the default APPS_DIR
		default_dir:
		snprintf(D_S(APPS_DIR), "%s%s", FAT_DRIVE, "/apps/USBLoader");
	}
	if (stat(APPS_DIR, &st) == 0) {
		chdir(APPS_DIR);
	} else {
		*APPS_DIR = 0;
	}
}   

// after cfg load
void cfg_setup1()
{
	// widescreen
	if (CFG.widescreen == CFG_WIDE_AUTO) {
		int widescreen = CONF_GetAspectRatio();
		if (widescreen) {
			CFG.widescreen = 1;
		} else {
			CFG.widescreen = 0;
		}
	}
}

// before vide & console init
void cfg_setup2()
{
	if (CFG.widescreen) {
		// copy over
		CONSOLE_XCOORD = CFG.W_CONSOLE_XCOORD;
		CONSOLE_YCOORD = CFG.W_CONSOLE_YCOORD;
		CONSOLE_WIDTH  = CFG.W_CONSOLE_WIDTH;
		CONSOLE_HEIGHT = CFG.W_CONSOLE_HEIGHT;
		COVER_XCOORD   = CFG.W_COVER_XCOORD;
		COVER_YCOORD   = CFG.W_COVER_YCOORD;
		strcopy(CFG.background, CFG.w_background, sizeof(CFG.background));
		if (CFG.W_COVER_WIDTH && CFG.W_COVER_HEIGHT) {
			COVER_WIDTH = CFG.W_COVER_WIDTH;
			COVER_HEIGHT = CFG.W_COVER_HEIGHT;
		} else {
			// automatic resize
			// normal (4:3): COVER_WIDTH = 160;
			// wide (16:9): COVER_WIDTH = 130;
			// ratio: *13/16
			// although true 4:3 -> 16:9 should be *12/16,
			// but it looks too compressed
			COVER_WIDTH = COVER_WIDTH * 13 / 16;
			// height is same
		}
	}
}

// This has to be called AFTER video & console init
void cfg_setup3()
{
	// auto set max_chars & entries
	int cols, rows;
	CON_GetMetrics(&cols, &rows);
	//MAX_CHARACTERS = cols - 4;
	MAX_CHARACTERS = cols - 2 - strlen(CFG.cursor);
	ENTRIES_PER_PAGE = rows - 5;
	// correct ENTRIES_PER_PAGE
	if (CFG.hide_header) ENTRIES_PER_PAGE++;
	if (CFG.hide_hddinfo) ENTRIES_PER_PAGE++;
	if (CFG.buttons == CFG_BTN_OPTIONS_1 || CFG.buttons == CFG_BTN_OPTIONS_B) {
		if (CFG.hide_footer) ENTRIES_PER_PAGE++;
	} else {
		ENTRIES_PER_PAGE--;
		if (CFG.hide_footer) ENTRIES_PER_PAGE+=2;
	}
}

// This has to be called BEFORE video & console init
void CFG_Load(int argc, char **argv)
{
	char pathname[200];
	char filename[200];
	struct stat st;
	bool try_sd = true;
	bool try_usb = false;
	bool ret;

	strcpy(FAT_DRIVE, "sd:");

	// are we started from usb?
	// then check usb first.
	if (argc && argv) {
		if (strncmp(argv[0], "usb:", 4) == 0) {
			if (Fat_MountUSB() == 0) {
				strcpy(FAT_DRIVE, "usb:");
				try_usb = true;
				try_sd = false;
			}
		}
	}

	retry:

	snprintf(D_S(USBLOADER_PATH), "%s%s", FAT_DRIVE, "/usb-loader");

	// Set current working directory to app path
	chdir_app(argc ? argv[0] : "");

	// setup defaults
  	CFG_Default();

	// load global config
	snprintf(D_S(filename), "%s/%s", USBLOADER_PATH, "config.txt");
	ret = cfg_parsefile(filename, &cfg_set);

	// try old location if default fails
	if (!ret) {
		snprintf(D_S(pathname), "%s%s", FAT_DRIVE, "/USBLoader");
		snprintf(D_S(filename), "%s/%s", pathname, "config.txt");
		if (stat(filename, &st) == 0) {
			// exists, use old base path
			STRCOPY(USBLOADER_PATH, pathname);
		  	CFG_Default(); // reset defaults with old path
			ret = cfg_parsefile(filename, &cfg_set);
		}
	}

	// still no luck? try APPS_DIR
	if (!ret && *APPS_DIR) {
		snprintf(D_S(filename), "%s/%s", APPS_DIR, "config.txt");
		if (stat(filename, &st) == 0) {
			// exists, use APPS_DIR as base
			STRCOPY(USBLOADER_PATH, APPS_DIR);
		  	CFG_Default(); // reset defaults with APPS_DIR
			ret = cfg_parsefile(filename, &cfg_set);
		}
	}

	// still no luck? try USB
	if (!ret) {
		if (!try_usb) {
			try_usb = true;
			if (Fat_MountUSB() == 0) {
				strcpy(FAT_DRIVE, "usb:");
				goto retry;
			}
		}
		else if (!try_sd) {
			try_sd = true;
			strcpy(FAT_DRIVE, "sd:");
			goto retry;
		} else {
			// still no dir found, just reset to sd:
			strcpy(FAT_DRIVE, "sd:");
		}
	}

	// load renamed titles
	snprintf(filename, sizeof(filename), "%s/%s", USBLOADER_PATH, "titles.txt");
	cfg_parsefile(filename, &title_set);

	// load per-app local config and titles
	// (/apps/USBLoader/config.txt and titles.txt if run from HBC)
	if (strcmp(USBLOADER_PATH, APPS_DIR) != 0) {
		cfg_parsefile("config.txt", &cfg_set);
		cfg_parsefile("titles.txt", &title_set);
	}

	// load per-game settings
	CFG_Load_Settings();

	// parse commandline arguments (wiiload)
	cfg_parsearg(argc, argv);

	// get theme list
	get_theme_list();

	// setup dependant and calculated parameters
	cfg_setup1();
	cfg_setup2();
}


void CFG_Setup(int argc, char **argv)
{
	cfg_setup3();
	cfg_debug(argc, argv);
}

#if 0

#include <malloc.h>
#include "wbfs.h"

int fake_num = 0;
int fake_max = 100;
struct discHdr *fake_list = NULL;

int is_fake(char *id)
{
	int i;
	for (i=0; i<fake_num;i++) {
		// ignore region, check only first 3
		if (strncmp((char*)fake_list[i].id, (char*)id, 3) == 0) return 1;
	}
	return 0;
}

// WBFS_GetCount
s32 dbg_WBFS_GetCount(u32 *count)
{
	DIR *dir;
	struct dirent *dent;
	char id[8], *p;
	int ret, cnt, len;

	fake_num = 0;
	if (fake_list) free(fake_list);
	fake_list = NULL;

	// first get the real list, then add fake entries
	ret = WBFS_GetCount((u32*)&cnt);
	if (ret >= 0) {
		len = sizeof(struct discHdr) * cnt;
		fake_list = (struct discHdr *)memalign(32, len);
		if (!fake_list) return -1;
		memset(fake_list, 0, len);
		ret = WBFS_GetHeaders(fake_list, cnt, sizeof(struct discHdr));
		if (ret >= 0) fake_num = cnt;
		//printf("real games num: %d\n", fake_num); sleep(2);
	}

	//printf("fake dir: %s\n", CFG.covers_path); sleep(1);
	dir = opendir(CFG.covers_path);
	if (!dir) {
		printf("fake dir error! %s\n", CFG.covers_path); sleep(2);
		return 0;
	}

	while ((dent = readdir(dir)) != NULL) {
		if (dent->d_name[0] == '.') continue;
		if (strstr(dent->d_name, ".png") == NULL
			&& strstr(dent->d_name, ".PNG") == NULL) continue;
		memset(id, 0, sizeof(id));
		STRCOPY(id, dent->d_name);
		p = strchr(id, '.');
		if (p == NULL) continue;
		*p = 0;
		// check if already exists, ignore region
		if (is_fake(id)) continue;
		fake_list = realloc(fake_list, sizeof(struct discHdr) * (fake_num+1));
		memset(fake_list+fake_num, 0, sizeof(struct discHdr));
		memcpy(fake_list[fake_num].id, id, sizeof(fake_list[fake_num].id));
		STRCOPY(fake_list[fake_num].title, dent->d_name);
		//printf("fake %d %.6s %s\n", fake_num,
		//		fake_list[fake_num].id, fake_list[fake_num].title);
		fake_num++;
		if (fake_num >= fake_max) break;
	}
	closedir(dir);
	sleep(2);
	*count = fake_num;
	return 0;
}

// WBFS_GetHeaders
s32 dbg_WBFS_GetHeaders(void *outbuf, u32 cnt, u32 len)
{
	memcpy(outbuf, fake_list, cnt*len);
	return 0;
}

#endif

