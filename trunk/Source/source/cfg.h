#include <gctypes.h>
#include "disc.h"
#include "util.h"

extern int ENTRIES_PER_PAGE;
extern int MAX_CHARACTERS;
extern int CONSOLE_XCOORD;
extern int CONSOLE_YCOORD;
extern int CONSOLE_WIDTH;
extern int CONSOLE_HEIGHT;
extern int CONSOLE_BG_COLOR;
extern int CONSOLE_FG_COLOR;
extern int COVER_XCOORD;
extern int COVER_YCOORD;
extern int COVER_WIDTH;
extern int COVER_HEIGHT;


#define CFG_LAYOUT_ORIG      0
#define CFG_LAYOUT_ORIG_12   1
#define CFG_LAYOUT_SMALL     2
#define CFG_LAYOUT_MEDIUM    3
#define CFG_LAYOUT_LARGE     4 // nixx
#define CFG_LAYOUT_LARGE_2   5 // usptactical
#define CFG_LAYOUT_LARGE_3   6 // oggzee
#define CFG_LAYOUT_ULTIMATE1 7 // Ultimate1: (WiiShizza)
#define CFG_LAYOUT_ULTIMATE2 8 // Ultimate2: (jservs7 / hungyip84)
#define CFG_LAYOUT_ULTIMATE3 9 // Ultimate3: (WiiShizza)
#define CFG_LAYOUT_KOSAIC   10 // Kosaic

#define CFG_VIDEO_SYS   0  // system default
#define CFG_VIDEO_AUTO  1
#define CFG_VIDEO_GAME  1  // game default
#define CFG_VIDEO_PATCH 2  // patch mode
#define CFG_VIDEO_PAL50	3  // force PAL
#define CFG_VIDEO_PAL60	4  // force PAL60
#define CFG_VIDEO_NTSC	5  // force NTSC
#define CFG_VIDEO_MAX   5

#define CFG_HOME_REBOOT 0
#define CFG_HOME_EXIT   1
#define CFG_HOME_SCRSHOT 2

//char languages[11][22] =
#define CFG_LANG_CONSOLE   0
#define CFG_LANG_JAPANESE  1
#define CFG_LANG_ENGLISH   2
#define CFG_LANG_GERMAN    3
#define CFG_LANG_FRENCH    4
#define CFG_LANG_SPANISH   5
#define CFG_LANG_ITALIAN   6
#define CFG_LANG_DUTCH     7
#define CFG_LANG_S_CHINESE 8
#define CFG_LANG_T_CHINESE 9
#define CFG_LANG_KOREAN   10
#define CFG_LANG_MAX      10

#define CFG_BTN_ORIGINAL  0 // obsolete
#define CFG_BTN_ULTIMATE  1 // obsolete
#define CFG_BTN_OPTIONS_1 2
#define CFG_BTN_OPTIONS_B 3

#define CFG_DEV_ASK  0
#define CFG_DEV_USB  1
#define CFG_DEV_SDHC 2

#define CFG_WIDE_NO   0
#define CFG_WIDE_YES  1
#define CFG_WIDE_AUTO 2

#define CFG_COLORS_MONO   1
#define CFG_COLORS_DARK   2
#define CFG_COLORS_BRIGHT 3

#define CFG_GUI_START 2

#define CFG_IOS_249       0
#define CFG_IOS_222_YAL   1
#define CFG_IOS_222_MLOAD 2
#define CFG_IOS_223_YAL   3
#define CFG_IOS_223_MLOAD 4
#define CFG_IOS_250       5
extern int CFG_IOS_MAX;
extern int CURR_IOS_IDX;
#define DEFAULT_IOS_IDX CFG_IOS_249
//#define DEFAULT_IOS_IDX CFG_IOS_222_MLOAD

extern char FAT_DRIVE[];
extern char USBLOADER_PATH[];

struct Game_CFG
{
	u8 id[8];
	int video;
	int language;
	int vidtv;
	int country_patch;
	int fix_002;
	int ios_idx;
	int block_ios_reload;
	int alt_dol;
	int ocarina;
};

struct CFG
{
	char background[200];
	char w_background[200];
	char bg_gui[200];
	char covers_path[200]; // currently used
	char covers_path_2d[200];
	char covers_path_3d[200];
	char covers_path_disc[200];
	int layout;
	int covers;
	// game options:
	struct Game_CFG game;
	// misc
	int home;
	int debug;
	int buttons;
	int device;
	int hide_header;
	// simple variants:
	int confirm_start;
	int hide_hddinfo;
	int hide_footer;
	int disable_format;
	int disable_remove;
	int disable_install;
	int disable_options;
	// end simple
	int install_all_partitions;
	// text colors
	int color_header;
	int color_selected_fg;
	int color_selected_bg;
	int color_inactive;
	int color_footer;
	int color_help;
	// music
	int music;
	char music_file[200];
	// widescreen
	int widescreen;
	int W_CONSOLE_XCOORD, W_CONSOLE_YCOORD;
	int W_CONSOLE_WIDTH, W_CONSOLE_HEIGHT;
	int W_COVER_XCOORD, W_COVER_YCOORD;
	int W_COVER_WIDTH, W_COVER_HEIGHT;
	// hide, pref games
	#define MAX_HIDE_GAME 200
	int num_hide_game;
	char hide_game[MAX_HIDE_GAME][8];
	// preferred games change sort order
	#define MAX_PREF_GAME 32
	int num_pref_game;
	char pref_game[MAX_PREF_GAME][8];
	// favorite games filter the list
	// 32 = 4*8 - one full page with max rows in gui mode
	#define MAX_FAVORITE_GAME 32
	int num_favorite_game;
	char favorite_game[MAX_FAVORITE_GAME][8];
	// cover urls
	char cover_url_norm[200];
	char cover_url_wide[200];
	int confirm_ocarina;
	int cursor_jump;
	int console_transparent;
	char cursor[8], cursor_space[8];
	char menu_plus[8], menu_plus_s[8];
	//int ios_idx;
	int ios, ios_yal, ios_mload;
	char theme[32];
	int gui;
	int gui_transit;
	int gui_style;
	int gui_rows;
	int gui_text_color;
};

extern struct CFG CFG;

extern int num_theme;
extern int cur_theme;

void CFG_Default();
void CFG_Load(int argc, char **argv);
void CFG_Setup(int argc, char **argv);
bool CFG_Load_Settings();
bool CFG_Save_Settings();
char *get_title(struct discHdr *header);
struct Game_CFG* CFG_get_game_opt(u8 *id);
bool CFG_save_game_opt(u8 *id);
bool CFG_forget_game_opt(u8 *id);
int CFG_hide_games(struct discHdr *list, int cnt);
void CFG_sort_pref(struct discHdr *list, int cnt);
void CFG_switch_theme(int theme_i);

char *ios_str(int idx);
void cfg_ios(char *name, char *val);
void cfg_ios_set_idx(int ios_idx);

bool set_favorite(u8 *id, bool fav);
bool is_favorite(u8 *id);
int CFG_filter_favorite(struct discHdr *list, int cnt);


