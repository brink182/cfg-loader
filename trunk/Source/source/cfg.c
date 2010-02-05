
// by oggzee & usptactical

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <ogcsys.h>
#include <dirent.h>
#include <malloc.h>

#include "cfg.h"
#include "wpad.h"
#include "gui.h"
#include "video.h"
#include "fat.h"
#include "net.h"
#include "xml.h"

char FAT_DRIVE[8] = "sd:";
char USBLOADER_PATH[200] = "sd:/usb-loader";
char APPS_DIR[200] = "";
char LAST_CFG_PATH[200];
char direct_start_id_buf[] = "#GAMEID\0\0\0\0\0CFGUSB0000000000";

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

int COVER_WIDTH_FRONT;
int COVER_HEIGHT_FRONT;

#define TITLE_MAX 64

struct ID_Title
{
	u8 id[8];
	char title[TITLE_MAX];
};

// renamed titles
int num_title = 0; //number of titles
struct ID_Title *cfg_title = NULL;

#define MAX_CFG_GAME 500
int num_cfg_game = 0;
struct Game_CFG_2 cfg_game[MAX_CFG_GAME];

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
	{ "pal50",  CFG_VIDEO_PAL50 },
	{ "pal60",  CFG_VIDEO_PAL60 },
	{ "ntsc",   CFG_VIDEO_NTSC },
	//{ "patch",  CFG_VIDEO_PATCH },
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
	{ "222-mload",  CFG_IOS_222_MLOAD },
	{ "223-mload",  CFG_IOS_223_MLOAD },
	{ "222-yal",    CFG_IOS_222_YAL },
	{ "223-yal",    CFG_IOS_223_YAL },
	{ "250",        CFG_IOS_250 },
	{ NULL, -1 }
};

struct TextMap map_gui_style[] =
{
	{ "grid",        GUI_STYLE_GRID },
	{ "flow",        GUI_STYLE_FLOW },
	{ "flow-z",      GUI_STYLE_FLOW_Z },
	{ "coverflow3d", GUI_STYLE_COVERFLOW + coverflow3d },
	{ "coverflow2d", GUI_STYLE_COVERFLOW + coverflow2d },
	{ "frontrow",    GUI_STYLE_COVERFLOW + frontrow },
	{ "vertical",    GUI_STYLE_COVERFLOW + vertical },
	{ "carousel",    GUI_STYLE_COVERFLOW + carousel },
	{ NULL, -1 }
};

struct playStat {
	char id[7];
	s32 playCount;
	time_t playTime;
};

int playStatsSize = 0;
struct playStat *playStats;

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
void cfg_direct_start(int argc, char **argv);


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
	//COVER_WIDTH      = 160; // set by cover_style
	//COVER_HEIGHT     = 225; // set by cover_style
	// widescreen
	CFG.W_CONSOLE_XCOORD = CONSOLE_XCOORD;
	CFG.W_CONSOLE_YCOORD = CONSOLE_YCOORD;
	CFG.W_CONSOLE_WIDTH  = CONSOLE_WIDTH;
	CFG.W_CONSOLE_HEIGHT = CONSOLE_HEIGHT;
	CFG.W_COVER_XCOORD   = COVER_XCOORD;
	CFG.W_COVER_YCOORD   = COVER_YCOORD;
	//CFG.W_COVER_WIDTH  = 0; // auto
	//CFG.W_COVER_HEIGHT = 0; // auto 
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
			CONSOLE_WIDTH    = 344;
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
	snprintf(D_S(CFG.covers_path_full), "%s/%s", CFG.covers_path, "full");
}

void cfg_set_cover_style(int style)
{
	CFG.cover_style = style;
	CFG.W_COVER_WIDTH  = 0; // auto
	CFG.W_COVER_HEIGHT = 0; // auto 
	switch (style) {
		case CFG_COVER_STYLE_2D:
			COVER_WIDTH = 160;
			COVER_HEIGHT = 225;
			break;
		case CFG_COVER_STYLE_3D:
			COVER_WIDTH = 160;
			COVER_HEIGHT = 225;
			break;
		case CFG_COVER_STYLE_DISC:
			COVER_WIDTH = 160;
			COVER_HEIGHT = 160;
			break;
		case CFG_COVER_STYLE_FULL:
			COVER_WIDTH = 512;
			if (CFG.gui_compress_covers)
				COVER_HEIGHT = 336;
			else
				COVER_HEIGHT = 340;
			//Calculate the width of the front cover only, given the cover dimensions above.
			// Divide the height by 1.4, which is the 2d cover height:width ratio (225x160 = 1.4)
			// and then divide by 4 and multiply by 4 so it's divisible.  This makes it easier if
			// we want to change the fullcover dimensions in the future.
			COVER_WIDTH_FRONT = (int)(COVER_HEIGHT / 1.4) >> 2 << 2;
			COVER_HEIGHT_FRONT = COVER_HEIGHT;
			break;
	}
}

void cfg_set_cover_style_str(char *val)
{
	if (strcmp(val, "standard")==0) {
		cfg_set_cover_style(CFG_COVER_STYLE_2D);
	} else if (strcmp(val, "3d")==0) {
		cfg_set_cover_style(CFG_COVER_STYLE_3D);
	} else if (strcmp(val, "disc")==0) {
		cfg_set_cover_style(CFG_COVER_STYLE_DISC);
	} else if (strcmp(val, "full")==0) {
		cfg_set_cover_style(CFG_COVER_STYLE_FULL);
	}
}

// setup cover style: path & url
void cfg_setup_cover_style()
{
	switch (CFG.cover_style) {
		case CFG_COVER_STYLE_3D:
			STRCOPY(CFG.covers_path, CFG.covers_path_3d); 
			STRCOPY(CFG.cover_url_norm, CFG.cover_url_3d_norm);
			//STRCOPY(CFG.cover_url_wide, CFG.cover_url_3d_wide);
			break;
		case CFG_COVER_STYLE_DISC:
			STRCOPY(CFG.covers_path, CFG.covers_path_disc); 
			STRCOPY(CFG.cover_url_norm, CFG.cover_url_disc_norm);
			//STRCOPY(CFG.cover_url_wide, CFG.cover_url_disc_wide);
			break;
		case CFG_COVER_STYLE_FULL:
			STRCOPY(CFG.covers_path, CFG.covers_path_full);
			STRCOPY(CFG.cover_url_norm, CFG.cover_url_full_norm);
			//STRCOPY(CFG.cover_url_wide, CFG.cover_url_full_norm);
			break;
		default:
		case CFG_COVER_STYLE_2D:
			STRCOPY(CFG.covers_path, CFG.covers_path_2d); 
			STRCOPY(CFG.cover_url_norm, CFG.cover_url_2d_norm);
			//STRCOPY(CFG.cover_url_wide, CFG.cover_url_2d_wide);
			break;
	}
}

	// OLD URLS:
	/*
	// www.theotherzone.com
	STRCOPY(CFG.cover_url_2d_norm,
			"http://www.theotherzone.com/wii/{REGION}/{ID6}.png");
	STRCOPY(CFG.cover_url_2d_wide,
			"http://www.theotherzone.com/wii/widescreen/{REGION}/{ID6}.png");
	STRCOPY(CFG.cover_url_3d_norm,
			"http://www.theotherzone.com/wii/3d/{WIDTH}/{HEIGHT}/{ID6}.png");
	STRCOPY(CFG.cover_url_3d_wide, CFG.cover_url_3d_norm);
	STRCOPY(CFG.cover_url_disc_norm,
			"http://www.theotherzone.com/wii/discs/{ID3}.png");
	STRCOPY(CFG.cover_url_disc_wide,
			"http://www.theotherzone.com/wii/diskart/{WIDTH}/{HEIGHT}/{ID3}.png");
	*/
	// www.wiiboxart.com
	// http://www.wiiboxart.com/pal/RJ2P52.png
	// http://www.wiiboxart.com/widescreen/pal/RJ2P52.png
	// http://www.wiiboxart.com/3d/160/225/RJ2P52.png
	// http://www.wiiboxart.com/disc/160/160/RJ2.png
	// http://www.wiiboxart.com/diskart/160/160/RJ2.png
	/*
	STRCOPY(CFG.cover_url_2d_norm,
			"http://www.wiiboxart.com/{REGION}/{ID6}.png");
	STRCOPY(CFG.cover_url_2d_wide,
			"http://www.wiiboxart.com/widescreen/{REGION}/{ID6}.png");
	STRCOPY(CFG.cover_url_3d_norm,
			"http://www.wiiboxart.com/3d/{WIDTH}/{HEIGHT}/{ID6}.png");
	STRCOPY(CFG.cover_url_3d_wide, CFG.cover_url_3d_norm);
	STRCOPY(CFG.cover_url_disc_norm,
			"http://www.wiiboxart.com/discs/{ID3}.png");
	STRCOPY(CFG.cover_url_disc_wide,
			"http://www.wiiboxart.com/diskart/{WIDTH}/{HEIGHT}/{ID3}.png");
	*/

void cfg_default_url()
{
	CFG.download_id_len = 6;
	CFG.download_all = 1;
	//strcpy(CFG.download_cc_pal, "EN");
	*CFG.download_cc_pal = 0; // means AUTO - console language

	*CFG.cover_url_2d_norm = 0;
	*CFG.cover_url_3d_norm = 0;
	*CFG.cover_url_disc_norm = 0;
	*CFG.cover_url_full_norm = 0;

	STRCOPY(CFG.cover_url_2d_norm,
		" http://wiitdb.com/wiitdb/artwork/cover/{CC}/{ID6}.png"
		" http://boxart.rowdyruff.net/flat/{ID6}.png"
		" http://www.muntrue.nl/covers/ALL/160/225/boxart/{ID6}.png"
		//" http://wiicover.gateflorida.com/sites/default/files/cover/2D%20Cover/{ID6}.png"
		//" http://awiibit.com/BoxArt160x224/{ID6}.png"
		);

	STRCOPY(CFG.cover_url_3d_norm,
		" http://wiitdb.com/wiitdb/artwork/cover3D/{CC}/{ID6}.png"
		" http://boxart.rowdyruff.net/3d/{ID6}.png"
		" http://www.muntrue.nl/covers/ALL/160/225/3D/{ID6}.png"
		//" http://wiicover.gateflorida.com/sites/default/files/cover/3D%20Cover/{ID6}.png"
		//" http://awiibit.com/3dBoxArt176x248/{ID6}.png"
		);

	STRCOPY(CFG.cover_url_disc_norm,
		" http://wiitdb.com/wiitdb/artwork/disc/{CC}/{ID6}.png"
		" http://boxart.rowdyruff.net/fulldisc/{ID6}.png"
		" http://www.muntrue.nl/covers/ALL/160/160/disc/{ID6}.png"
		//" http://wiicover.gateflorida.com/sites/default/files/cover/Disc%20Cover/{ID6}.png"
		//" http://awiibit.com/WiiDiscArt/{ID6}.png"
		);

	STRCOPY(CFG.cover_url_full_norm,
		" http://wiitdb.com/wiitdb/artwork/coverfull/{CC}/{ID6}.png"
		" http://www.muntrue.nl/covers/ALL/512/340/fullcover/{ID6}.png"
		//" http://wiicover.gateflorida.com/sites/default/files/cover/Full%20Cover/{ID6}.png"
		);
}


/**
 * Method that populates the coverflow global settings structure
 *  @return void
 */
void CFG_Default_Coverflow()
{
	CFG_cf_global.covers_3d = true;
	CFG_cf_global.number_of_covers = 7;
	CFG_cf_global.selected_cover = 1;
	CFG_cf_global.theme = coverflow3d;		// default to coverflow 3D
	
	CFG_cf_global.cover_back_xpos = 0;
	CFG_cf_global.cover_back_ypos = 0;
	CFG_cf_global.cover_back_zpos = -45;
	CFG_cf_global.cover_back_xrot = 0;
	CFG_cf_global.cover_back_yrot = 0;
	CFG_cf_global.cover_back_zrot = 0;
	CFG_cf_global.screen_fade_alpha = 210;
}

/**
 * Method that populates the coverflow theme settings structure
 *  @return void
 */
void CFG_Default_Coverflow_Themes()
{
	int i = 0;
	
	//allocate the structure array
	SAFE_FREE(CFG_cf_theme);
	CFG_cf_theme = malloc(5 * sizeof *CFG_cf_theme);	
	
	// coverflow 3D
		CFG_cf_theme[i].rotation_frames = 40;
		CFG_cf_theme[i].rotation_frames_fast = 7;
		CFG_cf_theme[i].cam_pos_x = 0.0;
		CFG_cf_theme[i].cam_pos_y = 0.0;
		CFG_cf_theme[i].cam_pos_z = 0.0;
		CFG_cf_theme[i].cam_look_x = 0.0;
		CFG_cf_theme[i].cam_look_y = 0.0;
		CFG_cf_theme[i].cam_look_z = -1.0;
		if (CFG.widescreen)
			CFG_cf_theme[i].number_of_side_covers = 4;
		else
			CFG_cf_theme[i].number_of_side_covers = 3;
		CFG_cf_theme[i].reflections_color_bottom = 0x666666FF;
		CFG_cf_theme[i].reflections_color_top = 0xAAAAAA33;
		CFG_cf_theme[i].alpha_rolloff = 0;
		CFG_cf_theme[i].floating_cover = false;
		CFG_cf_theme[i].title_text_xpos = -1;
		CFG_cf_theme[i].title_text_ypos = 434;
		CFG_cf_theme[i].cover_rolloff_x = 0;
		CFG_cf_theme[i].cover_rolloff_y = 0;
		CFG_cf_theme[i].cover_rolloff_z = 0;
		CFG_cf_theme[i].cover_center_xpos = 0;
		CFG_cf_theme[i].cover_center_ypos = 0;
		CFG_cf_theme[i].cover_center_zpos = -70;
		CFG_cf_theme[i].cover_center_xrot = 0;
		CFG_cf_theme[i].cover_center_yrot = 0;
		CFG_cf_theme[i].cover_center_zrot = 0;
		CFG_cf_theme[i].cover_center_reflection_used = true;
		CFG_cf_theme[i].cover_distance_from_center_left_x = -21;
		CFG_cf_theme[i].cover_distance_from_center_left_y = 0;
		CFG_cf_theme[i].cover_distance_from_center_left_z = 0;
		CFG_cf_theme[i].cover_distance_between_covers_left_x = -10;
		CFG_cf_theme[i].cover_distance_between_covers_left_y = 0;
		CFG_cf_theme[i].cover_distance_between_covers_left_z = 0;
		CFG_cf_theme[i].cover_left_xpos = 0;
		CFG_cf_theme[i].cover_left_ypos = 0;
		CFG_cf_theme[i].cover_left_zpos = -93;
		CFG_cf_theme[i].cover_left_xrot = 0;
		CFG_cf_theme[i].cover_left_yrot = 70;
		CFG_cf_theme[i].cover_left_zrot = 0;
		CFG_cf_theme[i].cover_distance_from_center_right_x = 21;
		CFG_cf_theme[i].cover_distance_from_center_right_y = 0;
		CFG_cf_theme[i].cover_distance_from_center_right_z = 0;
		CFG_cf_theme[i].cover_distance_between_covers_right_x = 10;
		CFG_cf_theme[i].cover_distance_between_covers_right_y = 0;
		CFG_cf_theme[i].cover_distance_between_covers_right_z = 0;
		CFG_cf_theme[i].cover_right_xpos = 0;
		CFG_cf_theme[i].cover_right_ypos = 0;
		CFG_cf_theme[i].cover_right_zpos = -93;
		CFG_cf_theme[i].cover_right_xrot = 0;
		CFG_cf_theme[i].cover_right_yrot = -70;
		CFG_cf_theme[i].cover_right_zrot = 0;
			
	
	// coverflow 2D
		i++;
		CFG_cf_theme[i].rotation_frames = 40;
		CFG_cf_theme[i].rotation_frames_fast = 7;
		CFG_cf_theme[i].cam_pos_x = 0.0;
		CFG_cf_theme[i].cam_pos_y = 0.0;
		CFG_cf_theme[i].cam_pos_z = 0.0;
		CFG_cf_theme[i].cam_look_x = 0.0;
		CFG_cf_theme[i].cam_look_y = 0.0;
		CFG_cf_theme[i].cam_look_z = -1.0;
		if (CFG.widescreen)
			CFG_cf_theme[i].number_of_side_covers = 4;
		else
			CFG_cf_theme[i].number_of_side_covers = 3;
		CFG_cf_theme[i].reflections_color_bottom = 0x666666FF;
		CFG_cf_theme[i].reflections_color_top = 0xAAAAAA33;
		CFG_cf_theme[i].alpha_rolloff = 0;
		CFG_cf_theme[i].floating_cover = false;
		CFG_cf_theme[i].title_text_xpos = -1;
		CFG_cf_theme[i].title_text_ypos = 434;
		CFG_cf_theme[i].cover_rolloff_x = 0;
		CFG_cf_theme[i].cover_rolloff_y = 0;
		CFG_cf_theme[i].cover_rolloff_z = 0;
		CFG_cf_theme[i].cover_center_xpos = 0;
		CFG_cf_theme[i].cover_center_ypos = 0;
		CFG_cf_theme[i].cover_center_zpos = -70;
		CFG_cf_theme[i].cover_center_xrot = 0;
		CFG_cf_theme[i].cover_center_yrot = 0;
		CFG_cf_theme[i].cover_center_zrot = 0;
		CFG_cf_theme[i].cover_center_reflection_used = true;
		CFG_cf_theme[i].cover_distance_from_center_left_x = -36;
		CFG_cf_theme[i].cover_distance_from_center_left_y = 0;
		CFG_cf_theme[i].cover_distance_from_center_left_z = 0;
		CFG_cf_theme[i].cover_distance_between_covers_left_x = -22;
		CFG_cf_theme[i].cover_distance_between_covers_left_y = 0;
		CFG_cf_theme[i].cover_distance_between_covers_left_z = 0;
		CFG_cf_theme[i].cover_left_xpos = 0;
		CFG_cf_theme[i].cover_left_ypos = 0;
		CFG_cf_theme[i].cover_left_zpos = -167;
		CFG_cf_theme[i].cover_left_xrot = 0;
		CFG_cf_theme[i].cover_left_yrot = 0;
		CFG_cf_theme[i].cover_left_zrot = 0;
		CFG_cf_theme[i].cover_distance_from_center_right_x = 36;
		CFG_cf_theme[i].cover_distance_from_center_right_y = 0;
		CFG_cf_theme[i].cover_distance_from_center_right_z = 0;
		CFG_cf_theme[i].cover_distance_between_covers_right_x = 22;
		CFG_cf_theme[i].cover_distance_between_covers_right_y = 0;
		CFG_cf_theme[i].cover_distance_between_covers_right_z = 0;
		CFG_cf_theme[i].cover_right_xpos = 0;
		CFG_cf_theme[i].cover_right_ypos = 0;
		CFG_cf_theme[i].cover_right_zpos = -167;
		CFG_cf_theme[i].cover_right_xrot = 0;
		CFG_cf_theme[i].cover_right_yrot = 0;
		CFG_cf_theme[i].cover_right_zrot = 0;
		
	// frontrow
		i++;
		CFG_cf_theme[i].rotation_frames = 40;
		CFG_cf_theme[i].rotation_frames_fast = 7;
		CFG_cf_theme[i].cam_pos_x = 0.0;
		CFG_cf_theme[i].cam_pos_y = 0.0;
		CFG_cf_theme[i].cam_pos_z = 0.0;
		CFG_cf_theme[i].cam_look_x = 0.0;
		CFG_cf_theme[i].cam_look_y = 0.0;
		CFG_cf_theme[i].cam_look_z = -1.0;
		CFG_cf_theme[i].number_of_side_covers = 4;
		CFG_cf_theme[i].reflections_color_bottom = 0x666666FF;
		CFG_cf_theme[i].reflections_color_top = 0xAAAAAA33;
		CFG_cf_theme[i].alpha_rolloff = 0;
		CFG_cf_theme[i].floating_cover = false;
		CFG_cf_theme[i].title_text_xpos = -1;
		CFG_cf_theme[i].title_text_ypos = 434;
		CFG_cf_theme[i].cover_rolloff_x = 0;
		CFG_cf_theme[i].cover_rolloff_y = 0;
		CFG_cf_theme[i].cover_rolloff_z = 0;
		CFG_cf_theme[i].cover_center_xpos = 0;
		CFG_cf_theme[i].cover_center_ypos = 0;
		CFG_cf_theme[i].cover_center_zpos = -70;
		CFG_cf_theme[i].cover_center_xrot = 0;
		CFG_cf_theme[i].cover_center_yrot = 0;
		CFG_cf_theme[i].cover_center_zrot = 0;
		CFG_cf_theme[i].cover_center_reflection_used = true;
		CFG_cf_theme[i].cover_distance_from_center_left_x = -27;
		CFG_cf_theme[i].cover_distance_from_center_left_y = 0;
		CFG_cf_theme[i].cover_distance_from_center_left_z = 47;
		CFG_cf_theme[i].cover_distance_between_covers_left_x = -35;
		CFG_cf_theme[i].cover_distance_between_covers_left_y = 0;
		CFG_cf_theme[i].cover_distance_between_covers_left_z = 80;
		CFG_cf_theme[i].cover_left_xpos = 0;
		CFG_cf_theme[i].cover_left_ypos = 0;
		CFG_cf_theme[i].cover_left_zpos = -87;
		CFG_cf_theme[i].cover_left_xrot = 0;
		CFG_cf_theme[i].cover_left_yrot = 0;
		CFG_cf_theme[i].cover_left_zrot = 0;
		if (CFG.widescreen) {
			CFG_cf_theme[i].cover_distance_from_center_right_x = 37;
			CFG_cf_theme[i].cover_distance_from_center_right_y = 0;
			CFG_cf_theme[i].cover_distance_from_center_right_z = -30;
			CFG_cf_theme[i].cover_distance_between_covers_right_x = 45;
			CFG_cf_theme[i].cover_distance_between_covers_right_y = 0;
			CFG_cf_theme[i].cover_distance_between_covers_right_z = -50;
		} else {
			CFG_cf_theme[i].cover_distance_from_center_right_x = 34;
			CFG_cf_theme[i].cover_distance_from_center_right_y = 0;
			CFG_cf_theme[i].cover_distance_from_center_right_z = -37;
			CFG_cf_theme[i].cover_distance_between_covers_right_x = 50;
			CFG_cf_theme[i].cover_distance_between_covers_right_y = 0;
			CFG_cf_theme[i].cover_distance_between_covers_right_z = -80;
		}
		CFG_cf_theme[i].cover_right_xpos = 0;
		CFG_cf_theme[i].cover_right_ypos = 0;
		CFG_cf_theme[i].cover_right_zpos = -87;
		CFG_cf_theme[i].cover_right_xrot = 0;
		CFG_cf_theme[i].cover_right_yrot = 0;
		CFG_cf_theme[i].cover_right_zrot = 0;
		
	// vertical
		i++;
		CFG_cf_theme[i].rotation_frames = 40;
		CFG_cf_theme[i].rotation_frames_fast = 7;
		CFG_cf_theme[i].cam_pos_x = 0.0;
		CFG_cf_theme[i].cam_pos_y = 0.0;
		CFG_cf_theme[i].cam_pos_z = 0.0;
		CFG_cf_theme[i].cam_look_x = 0.0;
		CFG_cf_theme[i].cam_look_y = 0.0;
		CFG_cf_theme[i].cam_look_z = -1.0;
		CFG_cf_theme[i].number_of_side_covers = 4;
		CFG_cf_theme[i].reflections_color_bottom = 0xFFFFFF00;
		CFG_cf_theme[i].reflections_color_top = 0xFFFFFF00;
		CFG_cf_theme[i].alpha_rolloff = 0;
		CFG_cf_theme[i].floating_cover = true;
		CFG_cf_theme[i].title_text_xpos = -1;
		CFG_cf_theme[i].title_text_ypos = 434;
		CFG_cf_theme[i].cover_rolloff_x = 0;
		CFG_cf_theme[i].cover_rolloff_y = 0;
		CFG_cf_theme[i].cover_rolloff_z = 0;
		CFG_cf_theme[i].cover_center_xpos = 8;
		CFG_cf_theme[i].cover_center_ypos = 0;
		CFG_cf_theme[i].cover_center_zpos = -85;
		CFG_cf_theme[i].cover_center_xrot = 0;
		CFG_cf_theme[i].cover_center_yrot = 0;
		CFG_cf_theme[i].cover_center_zrot = 0;
		CFG_cf_theme[i].cover_center_reflection_used = false;
		CFG_cf_theme[i].cover_distance_from_center_left_x = 0;
		CFG_cf_theme[i].cover_distance_from_center_left_y = -25;
		CFG_cf_theme[i].cover_distance_from_center_left_z = 0;
		CFG_cf_theme[i].cover_distance_between_covers_left_x = -25;
		CFG_cf_theme[i].cover_distance_between_covers_left_y = -5;
		CFG_cf_theme[i].cover_distance_between_covers_left_z = 0;
		CFG_cf_theme[i].cover_left_xpos = -15;
		CFG_cf_theme[i].cover_left_ypos = 0;
		CFG_cf_theme[i].cover_left_zpos = -130;
		CFG_cf_theme[i].cover_left_xrot = 0;
		CFG_cf_theme[i].cover_left_yrot = 0;
		CFG_cf_theme[i].cover_left_zrot = 0;
		CFG_cf_theme[i].cover_distance_from_center_right_x = 0;
		CFG_cf_theme[i].cover_distance_from_center_right_y = 25;
		CFG_cf_theme[i].cover_distance_from_center_right_z = 0;
		CFG_cf_theme[i].cover_distance_between_covers_right_x = -25;
		CFG_cf_theme[i].cover_distance_between_covers_right_y = 5;
		CFG_cf_theme[i].cover_distance_between_covers_right_z = 0;
		CFG_cf_theme[i].cover_right_xpos = -15;
		CFG_cf_theme[i].cover_right_ypos = 0;
		CFG_cf_theme[i].cover_right_zpos = -130;
		CFG_cf_theme[i].cover_right_xrot = 0;
		CFG_cf_theme[i].cover_right_yrot = 0;
		CFG_cf_theme[i].cover_right_zrot = 0;

	// carousel
		i++;
		CFG_cf_theme[i].rotation_frames = 40;
		CFG_cf_theme[i].rotation_frames_fast = 7;
		CFG_cf_theme[i].cam_pos_x = 0.0;
		CFG_cf_theme[i].cam_pos_y = 0.0;
		CFG_cf_theme[i].cam_pos_z = 0.0;
		CFG_cf_theme[i].cam_look_x = 0.0;
		CFG_cf_theme[i].cam_look_y = 0.0;
		CFG_cf_theme[i].cam_look_z = -1.0;
		CFG_cf_theme[i].number_of_side_covers = 4;
		CFG_cf_theme[i].reflections_color_bottom = 0x666666FF;
		CFG_cf_theme[i].reflections_color_top = 0xAAAAAA33;
		CFG_cf_theme[i].alpha_rolloff = 0;
		CFG_cf_theme[i].floating_cover = false;
		CFG_cf_theme[i].title_text_xpos = -1;
		CFG_cf_theme[i].title_text_ypos = 434;
		CFG_cf_theme[i].cover_rolloff_x = 0;
		CFG_cf_theme[i].cover_rolloff_y = 17;
		CFG_cf_theme[i].cover_rolloff_z = 0;
		CFG_cf_theme[i].cover_center_xpos = 0;
		CFG_cf_theme[i].cover_center_ypos = 0;
		CFG_cf_theme[i].cover_center_zpos = -122;
		CFG_cf_theme[i].cover_center_xrot = 0;
		CFG_cf_theme[i].cover_center_yrot = 0;
		CFG_cf_theme[i].cover_center_zrot = 0;
		CFG_cf_theme[i].cover_center_reflection_used = true;
		CFG_cf_theme[i].cover_distance_from_center_left_x = -22;
		CFG_cf_theme[i].cover_distance_from_center_left_y = 0;
		CFG_cf_theme[i].cover_distance_from_center_left_z = 0;
		CFG_cf_theme[i].cover_distance_between_covers_left_x = -22;
		CFG_cf_theme[i].cover_distance_between_covers_left_y = 0;
		CFG_cf_theme[i].cover_distance_between_covers_left_z = 0;
		CFG_cf_theme[i].cover_left_xpos = 0;
		CFG_cf_theme[i].cover_left_ypos = 0;
		CFG_cf_theme[i].cover_left_zpos = -120;
		CFG_cf_theme[i].cover_left_xrot = 0;
		CFG_cf_theme[i].cover_left_yrot = 12;
		CFG_cf_theme[i].cover_left_zrot = 0;
		CFG_cf_theme[i].cover_distance_from_center_right_x = 22;
		CFG_cf_theme[i].cover_distance_from_center_right_y = 0;
		CFG_cf_theme[i].cover_distance_from_center_right_z = 0;
		CFG_cf_theme[i].cover_distance_between_covers_right_x = 22;
		CFG_cf_theme[i].cover_distance_between_covers_right_y = 0;
		CFG_cf_theme[i].cover_distance_between_covers_right_z = 0;
		CFG_cf_theme[i].cover_right_xpos = 0;
		CFG_cf_theme[i].cover_right_ypos = 0;
		CFG_cf_theme[i].cover_right_zpos = -120;
		CFG_cf_theme[i].cover_right_xrot = 0;
		CFG_cf_theme[i].cover_right_yrot = -12;
		CFG_cf_theme[i].cover_right_zrot = 0;
/*
	// Bookshelf
		i++;
		CFG_cf_theme[i].rotation_frames = 17;
		CFG_cf_theme[i].rotation_frames_fast = 6;
		CFG_cf_theme[i].cam_pos_x = 0.0;
		CFG_cf_theme[i].cam_pos_y = 0.0;
		CFG_cf_theme[i].cam_pos_z = 0.0;
		CFG_cf_theme[i].cam_look_x = 0.0;
		CFG_cf_theme[i].cam_look_y = 0.0;
		CFG_cf_theme[i].cam_look_z = -1.0;
		if (CFG.widescreen)
			CFG_cf_theme[i].number_of_side_covers = 5;
		else
			CFG_cf_theme[i].number_of_side_covers = 5;
		CFG_cf_theme[i].reflections_color_bottom = 0x666666FF;
		CFG_cf_theme[i].reflections_color_top = 0xAAAAAA33;
		CFG_cf_theme[i].alpha_rolloff = 0;
		CFG_cf_theme[i].floating_cover = false;
		CFG_cf_theme[i].title_text_xpos = -1;
		CFG_cf_theme[i].title_text_ypos = 434;
		CFG_cf_theme[i].cover_rolloff_x = 0;
		CFG_cf_theme[i].cover_rolloff_y = 0;
		CFG_cf_theme[i].cover_rolloff_z = 0;
		CFG_cf_theme[i].cover_center_xpos = 1;
		CFG_cf_theme[i].cover_center_ypos = 0;
		CFG_cf_theme[i].cover_center_zpos = -75;
		CFG_cf_theme[i].cover_center_xrot = 0;
		CFG_cf_theme[i].cover_center_yrot = 0;
		CFG_cf_theme[i].cover_center_zrot = 0;
		CFG_cf_theme[i].cover_center_reflection_used = true;
		CFG_cf_theme[i].cover_distance_from_center_left_x = -13;
		CFG_cf_theme[i].cover_distance_from_center_left_y = 0;
		CFG_cf_theme[i].cover_distance_from_center_left_z = 0;
		CFG_cf_theme[i].cover_distance_between_covers_left_x = -2;
		CFG_cf_theme[i].cover_distance_between_covers_left_y = 0;
		CFG_cf_theme[i].cover_distance_between_covers_left_z = 0;
		CFG_cf_theme[i].cover_left_xpos = 0;
		CFG_cf_theme[i].cover_left_ypos = 0;
		CFG_cf_theme[i].cover_left_zpos = -75;
		CFG_cf_theme[i].cover_left_xrot = 0;
		CFG_cf_theme[i].cover_left_yrot = 90;
		CFG_cf_theme[i].cover_left_zrot = 0;
		CFG_cf_theme[i].cover_distance_from_center_right_x = 13;
		CFG_cf_theme[i].cover_distance_from_center_right_y = 0;
		CFG_cf_theme[i].cover_distance_from_center_right_z = 0;
		CFG_cf_theme[i].cover_distance_between_covers_right_x = 2;
		CFG_cf_theme[i].cover_distance_between_covers_right_y = 0;
		CFG_cf_theme[i].cover_distance_between_covers_right_z = 0;
		CFG_cf_theme[i].cover_right_xpos = 0;
		CFG_cf_theme[i].cover_right_ypos = 0;
		CFG_cf_theme[i].cover_right_zpos = -75;
		CFG_cf_theme[i].cover_right_xrot = 0;
		CFG_cf_theme[i].cover_right_yrot = 90;
		CFG_cf_theme[i].cover_right_zrot = 0;
*/
}

		
// theme controls the looks, not behaviour

void CFG_Default_Theme()
{
	*CFG.theme = 0;
	snprintf(D_S(CFG.background), "%s/%s", USBLOADER_PATH, "background.png");
	snprintf(D_S(CFG.w_background), "%s/%s", USBLOADER_PATH, "background_wide.png");
	snprintf(D_S(CFG.bg_gui), "%s/%s", USBLOADER_PATH, "bg_gui.png");
	STRCOPY(CFG.theme_path, USBLOADER_PATH);
	*CFG.bg_gui_wide = 0;
	CFG.covers   = 1;
	CFG.hide_header  = 0;
	CFG.hide_hddinfo = CFG_HIDE_HDDINFO;
	CFG.hide_footer  = 0;
	CFG.console_transparent = 0;
	CFG.buttons  = CFG_BTN_OPTIONS_1;
	strcpy(CFG.cursor, ">>");
	strcpy(CFG.cursor_space, "  ");
	strcpy(CFG.menu_plus,   "[+] ");
	strcpy(CFG.menu_plus_s, "    ");
	strcpy(CFG.favorite, "*");
	strcpy(CFG.saved, "#");

	CFG.gui_text.color = 0x000000FF; // black
	CFG.gui_text.outline = 0xFF; //0;
	CFG.gui_text.outline_auto = 1; //0;
	CFG.gui_text.shadow = 0;
	CFG.gui_text.shadow_auto = 0;

	CFG.gui_text2.color = 0xFFFFFFFF; // white
	CFG.gui_text2.outline = 0xFF;
	CFG.gui_text2.outline_auto = 1;
	CFG.gui_text2.shadow = 0;
	CFG.gui_text2.shadow_auto = 0;

	CFG.gui_title_top = 0;

	CFG.layout   = CFG_LAYOUT_LARGE_3;
	cfg_layout();
	cfg_set_cover_style(CFG_COVER_STYLE_2D);
	set_colors(CFG_COLORS_DARK);
}

void CFG_Default()
{
	memset(&CFG, 0, sizeof(CFG));

	snprintf(D_S(CFG.covers_path), "%s/%s", USBLOADER_PATH, "covers");
	cfg_set_covers_path();
	cfg_default_url();
	CFG_Default_Theme();
	snprintf(D_S(CFG.bg_gui_wide), "%s/%s", USBLOADER_PATH, "bg_gui_wide.png");
	//STRCOPY(CFG.gui_font, "font.png");
	STRCOPY(CFG.gui_font, "font_uni.png");
	
	CFG.home     = CFG_HOME_REBOOT;
	CFG.debug    = 0;
	CFG.device   = CFG_DEV_ASK;
	STRCOPY(CFG.partition, CFG_DEFAULT_PARTITION);
	CFG.confirm_start = 1;
	CFG.install_partitions = CFG_INSTALL_GAME; //CFG_INSTALL_ALL;
	CFG.disable_format = 0;
	CFG.disable_remove = 0;
	CFG.disable_install = 0;
	CFG.disable_options = 0;
	CFG.music = 1;
	CFG.widescreen = CFG_WIDE_AUTO;
	CFG.gui = 1;
	CFG.gui_rows = 2;
	CFG.admin_lock = 1;
	CFG.admin_mode_locked = 1;
	STRCOPY(CFG.unlock_password, CFG_UNLOCK_PASSWORD);
	CFG.gui_antialias = 4;
	CFG.gui_compress_covers = 1;
	// default game settings
	CFG.game.video    = CFG_VIDEO_AUTO;
	cfg_ios_set_idx(DEFAULT_IOS_IDX);
	// all other game settings are 0 (memset(0) above)
	STRCOPY(CFG.sort_ignore, "A,An,The");
	CFG.clock_style = 24;
	// profiles
	CFG.num_profiles = 1;
	CFG.current_profile = 0;
	STRCOPY(CFG.profile_names[0], "default");
	STRCOPY(CFG.titles_url, "http://wiitdb.com/titles.txt?LANG={CC}");
	CFG.intro = 1;
	CFG.fat_install_dir = 1;
	CFG.db_show_info = 1;
	CFG.write_playstats = 1;
	STRCOPY(CFG.db_url, "http://wiitdb.com/wiitdb.zip?LANG={DBL}");
	STRCOPY(CFG.db_language, get_cc());
	STRCOPY(CFG.sort, "title-asc");
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
void cfg_map_case(char *name, char *val, int *var, int id)
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

#define CFG_STR(name, var) cfg_str(name, var, sizeof(var))

bool cfg_str(char *name, char *var, int size)
{
	if (strcmp(name, cfg_name)==0) {
		if (strcmp(cfg_val,"0")==0) {
			*var = 0;
		} else {
			strcopy(var, cfg_val, size);
		}
		return true;
	}
	return false;
}

#define CFG_STR_LIST(name, var) cfg_str_list(name, var, sizeof(var))

// if val starting with + append to a space delimited list
bool cfg_str_list(char *name, char *var, int size)
{
	if (strcmp(name, cfg_name)==0) {
		if (cfg_val[0] == '+') {
			if (*var) {
				// append ' '
				strappend(var, " ", size);
			}
			// skip +
			char *val = cfg_val + 1;
			// trim space
			while (*val == ' ') val++;
			// append val
			strappend(var, val, size);
		} else {
			strcopy(var, cfg_val, size);
		}
		return true;
	}
	return false;
}

struct ID_Title* cfg_get_id_title(u8 *id)
{
	int i;
	// search 6 letter ID first
	for (i=0; i<num_title; i++) {
		if (memcmp(id, cfg_title[i].id, 6) == 0) {
			return &cfg_title[i];
		}
	}
	// and 4 letter ID next
	for (i=0; i<num_title; i++) {
		// only if it was ID4 in titles.txt
		if ((cfg_title[i].id[4] == 0)
			&& (memcmp(id, cfg_title[i].id, 4) == 0)) {
			return &cfg_title[i];
		}
	}
	return NULL;
}

char *cfg_get_title(u8 *id)
{
	struct ID_Title *idt = cfg_get_id_title(id);
	if (idt) return idt->title;
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
	struct ID_Title *idt = cfg_get_id_title((u8*)id);
	if (idt && strncmp(id, (char*)idt->id, 6) == 0) {
		// replace
		strcopy(idt->title, title, TITLE_MAX);
	} else {
		//cfg_title = realloc(cfg_title, (num_title+1) * sizeof(struct ID_Title));
		cfg_title = mem1_realloc(cfg_title, (num_title+1) * sizeof(struct ID_Title));
		if (!cfg_title) {
			// error
			num_title = 0;
			return;
		}
		// add
		memset(&cfg_title[num_title], 0, sizeof(cfg_title[num_title]));
		strcopy((char*)cfg_title[num_title].id, id, 7);
		strcopy(cfg_title[num_title].title, title, TITLE_MAX);
		num_title++;
	}
}

// trim leading and trailing whitespace
// copy at max n or at max size-1
char* trimcopy_n(char *dest, char *src, int n, int size)
{
	int len;
	// trim leading white space
	while (isspace(*src) && n>0) { src++; n--; }
	len = strlen(src);
	if (len > n) len = n;
	// trim trailing white space
	while (len > 0 && isspace(src[len-1])) len--;
	// limit length
	if (len >= size) len = size-1;
	// safety
	if (len < 0) len = 0;
	strncpy(dest, src, len);
	dest[len] = 0;
	//printf("trim_copy: '%s' %d\n", dest, len); //sleep(1);
	return dest;
}

char* trimcopy(char *dest, char *src, int size)
{
	return trimcopy_n(dest, src, size, size);
}

// returns ptr to next token
// note: omits empty tokens
// on last token returns null
char* split_token(char *dest, char *src, char delim, int size)
{
	char *next;
	start:
	next = strchr(src, delim);
	if (next) {
		trimcopy_n(dest, src, next-src, size);
		next++;
	} else {
		trimcopy(dest, src, size);
	}
	if (next && *dest == 0) {
		// omit empty tokens
		src = next;
		goto start;
	}
	return next;
}

// on last token returns ptr to end of string
// when no more tokens returns null
char* split_tokens(char *dest, char *src, char *delim, int size)
{
	char *next;
	start:
	// end of string?
	if (*src == 0) return NULL;
	// strcspn:
	// returns the number of characters of str1 read before this first occurrence.
	// The search includes the terminating null-characters, so the function will
	// return the length of str1 if none of the characters of str2 are found in str1.
	next = src + strcspn(src, delim);
	if (*next) {
		trimcopy_n(dest, src, next-src, size);
		next++;
	} else {
		trimcopy(dest, src, size);
	}
	if (*dest == 0) {
		// omit empty tokens
		src = next;
		goto start;
	}
	return next;
}

// split line to part1 delimiter part2 
bool trimsplit(char *line, char *part1, char *part2, char delim, int size)
{
	char *eq = strchr(line, delim);
	if (!eq) return false;
	trimcopy_n(part1, line, eq-line, size);
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

bool copy_theme_path(char *dest, char *val, int size)
{
	// check if it's an absolute path (contains : as in sd:/...)
	if (strchr(val, ':')) {
		// absolute path with drive (sd:/images/...)
		strcopy(dest, val, size);
		return true;
	} else if (val[0] == '/') {
		// absolute path without drive (/images/...)
		snprintf(dest, size, "%s%s", FAT_DRIVE, val);
		return true;
	} else if (*theme_path) {
		struct stat st;
		snprintf(dest, size, "%s/%s", theme_path, val);
		if (stat(dest, &st) == 0) return true;
	}
	snprintf(dest, size, "%s/%s", USBLOADER_PATH, val);
	return false;
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

void cfg_id_list(char *name, char list[][8], int *num_list, int max_list)
{
	int i;
	
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
				
				i = find_in_game_list((u8*)id, list, *num_list);
				if (i < 0) {
					strcopy(list[*num_list], id, 8);
					(*num_list)++;
				}
			}
		}
	}
}

bool cfg_color(char *name, int *var)
{
	if (cfg_map(name, "black", var, 0x000000FF)) return true;
	if (cfg_map(name, "white", var, 0xFFFFFFFF)) return true;
	if (cfg_int_hex(name, var)) return true;
	return false;
}

// text color
void font_color_set(char *base_name, struct FontColor *fc)
{
	if (strncmp(cfg_name, base_name, strlen(base_name)) != 0) return;

	char name[100];
	char *old_name;
	STRCOPY(name, cfg_name);
	str_replace(name, base_name, "gui_text_", sizeof(name));
	old_name = cfg_name;
	cfg_name = name;

	cfg_color("gui_text_color", &fc->color);

	if (cfg_color("gui_text_outline", &fc->outline)) {
		if (strlen(cfg_val) == 2) fc->outline_auto = 1;
	}

	if (cfg_color("gui_text_shadow", &fc->shadow)) {
		if (strlen(cfg_val) == 2) fc->shadow_auto = 1;
	}

	cfg_name = old_name;
}


// theme options

void theme_set_base(char *name, char *val)
{
	cfg_name = name;
	cfg_val = val;
	cfg_bool("hide_header",  &CFG.hide_header);
	cfg_bool("hide_hddinfo", &CFG.hide_hddinfo);
	cfg_bool("hide_footer",  &CFG.hide_footer);
	cfg_bool("db_show_info",  &CFG.db_show_info);
	cfg_bool("write_playstats",  &CFG.write_playstats);
	
	cfg_map("buttons", "original",   &CFG.buttons, CFG_BTN_ORIGINAL);
	cfg_map("buttons", "options",    &CFG.buttons, CFG_BTN_OPTIONS_1);
	cfg_map("buttons", "options_1",  &CFG.buttons, CFG_BTN_OPTIONS_1);
	cfg_map("buttons", "options_B",  &CFG.buttons, CFG_BTN_OPTIONS_B);
	// if simple is specified in theme it only affects the looks,
	// does not lock down admin modes
	int simpl;
	if (cfg_bool("simple",  &simpl)) {
		if (simpl == 1) {
			if (CFG_HIDE_HDDINFO == 0) {
				// normal version affects hddinfo
				// fat version does not change it.
				CFG.hide_hddinfo = 1;
			}
			CFG.hide_hddinfo = 1;
			CFG.hide_footer = 1;
		} else { // simple == 0
			if (CFG_HIDE_HDDINFO == 0) {
				// normal version affects hddinfo
				// fat version does not change it.
				CFG.hide_hddinfo = 0;
			}
			CFG.hide_footer = 0;
		}
	}
	if (strcmp(name, "cover_style")==0) {
		cfg_set_cover_style_str(val);
	}
	if (strcmp(name, "cursor")==0) {
		//unquote(CFG.cursor, val, 2+1);
		unquote(CFG.cursor, val, sizeof(CFG.cursor));
		mbs_trunc(CFG.cursor, 2);
		int len = mbs_len(CFG.cursor);
		memset(CFG.cursor_space, ' ', len);
		CFG.cursor_space[len] = 0;
	}
	if (strcmp(name, "menu_plus")==0) {
		unquote(CFG.menu_plus, val, sizeof(CFG.menu_plus));
		mbs_trunc(CFG.menu_plus, 4);
		int len = mbs_len(CFG.menu_plus);
		memset(CFG.menu_plus_s, ' ', len);
		CFG.menu_plus_s[len] = 0;
	}
	font_color_set("gui_text_", &CFG.gui_text);
	font_color_set("gui_text2_", &CFG.gui_text2);
	cfg_bool("gui_title_top", &CFG.gui_title_top);
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
}

void theme_set(char *name, char *val)
{
	cfg_name = name;
	cfg_val = val;

	theme_set_base(name, val);

	if (cfg_map_auto("layout", map_layout, &CFG.layout)) {
		cfg_layout();
	}

	cfg_bool("covers",  &CFG.covers);
	cfg_bool("console_transparent", &CFG.console_transparent);

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

	if (strcmp(name, "background")==0
		|| strcmp(name, "background_base")==0)
	{
		if (strcmp(val, "0")==0) {
			*CFG.background = 0;
		} else {
			copy_theme_path(CFG.background, val, sizeof(CFG.background));
		}
	}

	if (strcmp(name, "wbackground")==0
		|| strcmp(name, "wbackground_base")==0)
	{
		if (strcmp(val, "0")==0) {
			*CFG.w_background = 0;
		} else {
			copy_theme_path(CFG.w_background, val, sizeof(CFG.w_background));
		}
	}

	if (strcmp(name, "background_gui")==0)
	{
		if (strcmp(val, "0")==0) {
			*CFG.bg_gui = 0;
		} else {
			copy_theme_path(CFG.bg_gui, val, sizeof(CFG.bg_gui));
		}
	}

	if (strcmp(name, "wbackground_gui")==0)
	{
		if (strcmp(val, "0")==0) {
			*CFG.bg_gui_wide = 0;
		} else {
			copy_theme_path(CFG.bg_gui_wide, val, sizeof(CFG.bg_gui_wide));
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
	int i;
	if (!cfg_ios_idx(name, val, &i)) return;
	cfg_ios_set_idx(i);
	/*
	int ios;
	if (sscanf(val, "%d", &ios) != 1) return;
	CFG.game.ios_idx = i;
	CFG.ios = ios;
	CFG.ios_yal = 0;
	CFG.ios_mload = 0;
	if (strstr(val, "-yal")) {
		CFG.ios_yal = 1;
	}
	if (strstr(val, "-mload")) {
		CFG.ios_yal = 1;
		CFG.ios_mload = 1;
	}
	*/
}

void cfg_set_game(char *name, char *val, struct Game_CFG *game_cfg)
{
	cfg_name = name;
	cfg_val = val;

	cfg_map_auto("language", map_language, &game_cfg->language);

	cfg_map_auto("video", map_video, &game_cfg->video);
	if (strcmp("video", name) == 0 && strcmp("vidtv", val) == 0)
	{
		game_cfg->video = CFG_VIDEO_AUTO;
		game_cfg->vidtv = 1;
	}
	if (strcmp("video", name) == 0 && strcmp("patch", val) == 0)
	{
		game_cfg->video = CFG_VIDEO_AUTO;
		game_cfg->video_patch = 1;
	}

	cfg_bool("video_patch", &game_cfg->video_patch);
	cfg_map ("video_patch", "all", &game_cfg->video_patch, CFG_VIDEO_PATCH_ALL);

	cfg_bool("vidtv", &game_cfg->vidtv);
	cfg_bool("country_patch", &game_cfg->country_patch);
	cfg_bool("fix_002", &game_cfg->fix_002);
	cfg_ios_idx(name, val, &game_cfg->ios_idx);
	cfg_bool("block_ios_reload", &game_cfg->block_ios_reload);
	if (strcmp("alt_dol", name) == 0) {
		int set = 0;
		if (cfg_bool("alt_dol", &game_cfg->alt_dol)) set = 1;
		if (cfg_map ("alt_dol", "sd", &game_cfg->alt_dol, 1)) set = 1;
		if (cfg_map ("alt_dol", "disc", &game_cfg->alt_dol, 2)) set = 1;
		if (!set) {
			// name specified
			game_cfg->alt_dol = 2;
			STRCOPY(game_cfg->dol_name, val);
		}
	}
	if (strcmp("dol_name", name) == 0 && *val) {
		STRCOPY(game_cfg->dol_name, val);
	}
	cfg_bool("ocarina", &game_cfg->ocarina);
}

bool cfg_set_gbl(char *name, char *val)
{
	if (cfg_map("device", "ask",  &CFG.device, CFG_DEV_ASK)) return true;
	if (cfg_map("device", "usb",  &CFG.device, CFG_DEV_USB)) return true;
	if (cfg_map("device", "sdhc", &CFG.device, CFG_DEV_SDHC)) return true;

	CFG_STR("partition", CFG.partition);

	if (cfg_map_auto("gui_style", map_gui_style, &CFG.gui_style)) return true;

	int rows = 0;
	if (cfg_int_max("gui_rows", &rows, 4)) {
		if (rows > 0) {
			CFG.gui_rows = rows;
			return true;
		}
	}
	CFG_STR("profile", CFG.current_profile_name);

	// theme must be last, because it changes cfg_name, cfg_val
	if (strcmp(name, "theme")==0) {
		if (*val) {
			load_theme(val);
			return true;
		}
	}
	return false;
}

void cfg_set_early(char *name, char *val)
{
	cfg_name = name;
	cfg_val = val;
	//cfg_bool("debug",   &CFG.debug);
	cfg_int_max("debug",   &CFG.debug, 255);
	cfg_bool("widescreen", &CFG.widescreen);
	cfg_map("widescreen", "auto", &CFG.widescreen, CFG_WIDE_AUTO);
	cfg_ios(name, val);
	CFG_STR("partition", CFG.partition);
	cfg_bool("intro", &CFG.intro);
}

void cfg_set(char *name, char *val)
{
	cfg_name = name;
	cfg_val = val;

	theme_set(name, val);

	cfg_set_early(name, val);

	cfg_bool("start_favorites", &CFG.start_favorites);
	cfg_bool("confirm_start", &CFG.confirm_start);
	cfg_bool("confirm_ocarina", &CFG.confirm_ocarina);
	
	cfg_bool("disable_format", &CFG.disable_format); 
	cfg_bool("disable_remove", &CFG.disable_remove); 
	cfg_bool("disable_install", &CFG.disable_install);
	cfg_bool("disable_options", &CFG.disable_options);
	
	cfg_bool("admin_lock", &CFG.admin_lock);
	cfg_bool("admin_unlock", &CFG.admin_lock);
	if (strcmp(name, "unlock_password")==0) {
		STRCOPY(CFG.unlock_password, val);
	}
	
	cfg_map("home", "exit",   &CFG.home, CFG_HOME_EXIT);
	cfg_map("home", "reboot", &CFG.home, CFG_HOME_REBOOT);
	cfg_map("home", "hbc",    &CFG.home, CFG_HOME_HBC);
	cfg_map("home", "screenshot", &CFG.home, CFG_HOME_SCRSHOT);

	cfg_int_max("cursor_jump", &CFG.cursor_jump, 50);
	cfg_bool("console_mark_page", &CFG.console_mark_page);
	cfg_bool("console_mark_favorite", &CFG.console_mark_favorite);
	cfg_bool("console_mark_saved", &CFG.console_mark_saved);

	// db options - Lustar
	if (!strcmp(name, "db_url")) 
		strcpy(CFG.db_url,val);
	if (!strcmp(name, "db_language")) {
		if (strcmp(val, "auto") == 0 || strcmp(val, "AUTO") == 0) {
			STRCOPY(CFG.db_language, get_cc());
		} else {
			strcpy(CFG.db_language,val);
		}
	}
	if (!strcmp(name, "sort"))
		strcpy(CFG.sort,val);
	cfg_bool("gui_compress_covers", &CFG.gui_compress_covers);	
	cfg_int_max("gui_antialias", &CFG.gui_antialias, 4);
	if (CFG.gui_antialias==0 || !CFG.gui_compress_covers) CFG.gui_antialias = 1;
	if (CFG.gui_antialias > 1) CFG.gui_compress_covers = 1;

	if (cfg_set_gbl(name, val)) {
		return;
	}

	cfg_set_game(name, val, &CFG.game);
	if (!CFG.direct_launch) {
		*CFG.game.dol_name = 0;
	}

	// covers path
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
	if (strcmp(name, "covers_path_full")==0) {
		COPY_PATH(CFG.covers_path_full, val);
	}

	// urls
	CFG_STR("titles_url", CFG.titles_url);
	// url_2d
	CFG_STR_LIST("cover_url", CFG.cover_url_2d_norm);
	CFG_STR_LIST("cover_url_norm", CFG.cover_url_2d_norm);
	//CFG_STR_LIST("cover_url_wide", CFG.cover_url_2d_wide);
	// url_3d
	CFG_STR_LIST("cover_url_3d", CFG.cover_url_3d_norm);
	CFG_STR_LIST("cover_url_3d_norm", CFG.cover_url_3d_norm);
	//CFG_STR_LIST("cover_url_3d_wide", CFG.cover_url_3d_wide);
	// url_disc
	CFG_STR_LIST("cover_url_disc", CFG.cover_url_disc_norm);
	CFG_STR_LIST("cover_url_disc_norm", CFG.cover_url_disc_norm);
	//CFG_STR_LIST("cover_url_disc_wide", CFG.cover_url_disc_wide);
	// url_full
	CFG_STR_LIST("cover_url_full", CFG.cover_url_full_norm);
	CFG_STR_LIST("cover_url_full_norm", CFG.cover_url_full_norm);
	// download options
	cfg_bool("download_all_styles", &CFG.download_all);
	//cfg_bool("download_wide", &CFG.download_wide); // OBSOLETE
	cfg_map("download_id_len", "4", &CFG.download_id_len, 4);
	cfg_map("download_id_len", "6", &CFG.download_id_len, 6);
	if (strcmp("download_cc_pal", name) == 0) {
		if (strcmp(val, "auto") == 0 || strcmp(val, "AUTO") == 0) {
			*CFG.download_cc_pal = 0;
		} else if (strlen(val) == 2) {
			strcpy(CFG.download_cc_pal, val);
		}
	}

	// gui
	cfg_bool("gui", &CFG.gui);
	cfg_map("gui", "start", &CFG.gui, CFG_GUI_START);

	cfg_map("gui_transition", "scroll", &CFG.gui_transit, 0);
	cfg_map("gui_transition", "fade",   &CFG.gui_transit, 1);
	CFG_STR("gui_font", CFG.gui_font);
	cfg_bool("gui_lock", &CFG.gui_lock);

	// simple changes dependant options
	int simpl;
	if (cfg_bool("simple",  &simpl)) {
		if (simpl == 1) {
			CFG.confirm_start = 0;
			if (CFG_HIDE_HDDINFO == 0) {
				// normal version affects hddinfo
				// fat version does not change it.
				CFG.hide_hddinfo = 1;
			}
			CFG.hide_footer = 1;
			CFG.disable_remove = 1;
			CFG.disable_install = 1;
			CFG.disable_options = 1;
			CFG.disable_format = 1;
		} else { // simple == 0
			CFG.confirm_start = 1;
			if (CFG_HIDE_HDDINFO == 0) {
				// normal version affects hddinfo
				// fat version does not change it.
				CFG.hide_hddinfo = 0;
			}
			CFG.hide_footer = 0;
			CFG.disable_remove = 0;
			CFG.disable_install = 0;
			CFG.disable_options = 0;
			CFG.disable_format = 0;
		}
	}
	cfg_map("install_partitions", "only_game",
			&CFG.install_partitions, CFG_INSTALL_GAME);
	cfg_map("install_partitions", "all",
			&CFG.install_partitions, CFG_INSTALL_ALL);
	cfg_map("install_partitions", "1:1",
			&CFG.install_partitions, CFG_INSTALL_1_1);

	extern u64 OPT_split_size;
	int split_size = 0;
	if (cfg_int_max("fat_split_size", &split_size, 4)) {
		if (split_size == 2) {
			OPT_split_size = (u64)2LL * 1024 * 1024 * 1024 - 32 * 1024;
		}
		if (split_size == 4) {
			OPT_split_size = (u64)4LL * 1024 * 1024 * 1024 - 32 * 1024;
		}
	}

	cfg_int_max("fat_install_dir", &CFG.fat_install_dir, 2);
	cfg_bool("disable_nsmb_patch", &CFG.disable_nsmb_patch);
	cfg_bool("disable_wip", &CFG.disable_wip);
	cfg_bool("disable_bca", &CFG.disable_bca);

	cfg_id_list("hide_game", CFG.hide_game, &CFG.num_hide_game, MAX_HIDE_GAME);
	cfg_id_list("pref_game", CFG.pref_game, &CFG.num_pref_game, MAX_PREF_GAME);
	cfg_id_list("favorite_game", CFG.favorite_game,
			&CFG.num_favorite_game, MAX_FAVORITE_GAME);
	CFG_STR("sort_ignore", CFG.sort_ignore);

	cfg_map("clock_style", "0",  &CFG.clock_style, 0);
	cfg_map("clock_style", "24", &CFG.clock_style, 24);
	cfg_map("clock_style", "12", &CFG.clock_style, 12);
	cfg_map("clock_style", "12am", &CFG.clock_style, 13);

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

	if (strcmp(name, "profile_names")==0) {
		char *next;
		next = val;
		CFG.num_profiles = 0;
		while ((next = split_tokens(
				CFG.profile_names[CFG.num_profiles],
				next, ",", sizeof(CFG.profile_names[0]))
				))
		{
			if (*CFG.profile_names[CFG.num_profiles] == 0) break;
			CFG.num_profiles++;
			if (CFG.num_profiles >= MAX_PROFILES) break;
		}
		if (CFG.num_profiles == 0) {
			CFG.num_profiles = 1;
			STRCOPY(CFG.profile_names[0], "default");
		}
	}
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
		return CFG_Save_Settings();
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

bool is_hide_game(u8 *id)
{
	return is_in_game_list(id, CFG.hide_game,
			CFG.num_hide_game);
}

bool set_hide_game(u8 *id, bool hide)
{
	int i;
	bool ret;
	i = find_in_game_list(id, CFG.hide_game, CFG.num_hide_game);
	if (i >= 0) {
		// already in the list
		if (hide) return true;
		// remove
		remove_from_list(i, CFG.hide_game, &CFG.num_hide_game);
		return CFG_Save_Settings();
	}
	// not on list
	if (!hide) return true;
	// add
	ret = add_to_list(id, CFG.hide_game, &CFG.num_hide_game, MAX_HIDE_GAME);
	if (!ret) return ret;
	return CFG_Save_Settings();
}


// split line to name = val and trim whitespace
void cfg_parseline(char *line, void (*set_func)(char*, char*))
{
	// split name = value
	char name[400], val[400];
	// handle [group=param] as name=[group] val=param
	// '=param' is optional
	if (*line == '[') {
		trimcopy(name, line, sizeof(name));
		if (name[strlen(name)-1] != ']') return;
		// check for optional =param
		*val = 0;
		char *eq = strchr(name, '=');
		if (eq) {
			trimcopy(val, eq+1, sizeof(val));
			if (val[strlen(val)-1] == ']') {
				val[strlen(val)-1] = 0;
			}
			strcpy(eq, "]");
		}
	} else {
		if (!trimsplit(line, name, val, '=', sizeof(name))) return;
	}
	//printf("CFG: '%s=%s'\n", name, val); //sleep(1);
	set_func(name, val);
}

bool cfg_parsebuf(char *buf, void (*set_func)(char*, char*))
{
	char line[500];
	char *p, *nl;
	int len;

	nl = buf;
	for (;;) {
		p = nl;
		if (p == NULL) break;
		while (*p == '\n') p++;
		if (*p == 0) break;
		nl = strchr(p, '\n');
		if (nl == NULL) {
			len = strlen(p);
		} else {
			len = nl - p;
		}
		// lines starting with # are comments
		if (*p == '#') continue;
		if (len >= sizeof(line)) len = sizeof(line) - 1;
		strcopy(line, p, len+1);
		cfg_parseline(line, set_func);
	}
	return true;
}

bool cfg_parsefile(char *fname, void (*set_func)(char*, char*))
{
	FILE *f;
	char line[500];
	char bom[] = {0xEF, 0xBB, 0xBF};
	int first_line = 1;

	//printf("opening(%s)\n", fname); sleep(3);
	f = fopen(fname, "rb");
	if (!f) {
		//printf("error opening(%s)\n", fname); sleep(3);
		return false;
	}

	while (fgets(line, sizeof(line), f)) {
		// skip BOM UTF-8 (ef bb bf)
		if (first_line) {
			if (memcmp(line, bom, sizeof(bom)) == 0) {
				memmove(line, line+sizeof(bom), strlen(line)-sizeof(bom)+1);
				/*printf("BOM found in %s\n", fname);
				printf("line: '%s'\n", line);
				sleep(3);*/
			}
			first_line = 0;
		}
		// lines starting with # are comments
		if (line[0] == '#') continue;
		// parse
		cfg_parseline(line, set_func);
	}
	fclose(f);
	return true;
}

void cfg_parsearg(int argc, char **argv)
{
	int i;
	char pathname[200];
	bool is_opt;
	for (i=1; i<argc; i++) {
		//printf("arg[%d]: %s\n", i, argv[i]);
		is_opt = strchr(argv[i], '=') || strchr(argv[i], '[');
		if (is_opt) {
			cfg_parseline(argv[i], &cfg_set);
		} else if (argv[i][0] != '#') {
			snprintf(pathname, sizeof(pathname), "%s/%s", USBLOADER_PATH, argv[i]);
			cfg_parsefile(pathname, &cfg_set);
		}
	}
}

void cfg_parsearg_early(int argc, char **argv)
{
	int i;
	char *eq;
	// setup defaults
	CFG_Default();
	// check for direct start
	cfg_direct_start(argc, argv);
	// parse the rest
	for (i=1; i<argc; i++) {
		eq = strchr(argv[i], '=');
		if (eq) {
			cfg_parseline(argv[i], &cfg_set_early);
		}
	}
}


// SETTINGS.CFG

int cfg_find_profile(char *pname)
{
	int i;
	for (i=0; i<CFG.num_profiles; i++) {
		if (strcasecmp(CFG.profile_names[i], pname) == 0) {
			return i;
		}
	}
	if (strcasecmp(pname, "default") == 0) return 0;
	return -1;
}

int profile_tag_index = 0;

void settings_set(char *name, char *val)
{
	cfg_name = name;
	cfg_val = val;
	//printf("s: %s : %s\n", name, val);
	if (strcmp(name, "[profile]") == 0) {
		int i = cfg_find_profile(val);
		CFG.current_profile = i >= 0 ? i : 0;
		profile_tag_index = i;
		//printf("%s : %s : %d\n", name, val, i);
	}
	if (profile_tag_index >= 0) {
		cfg_id_list("favorite_game", CFG.favorite_game,
			&CFG.num_favorite_game, MAX_FAVORITE_GAME);
	}
	cfg_id_list("hide_game", CFG.hide_game,
			&CFG.num_hide_game, MAX_HIDE_GAME);
	game_set(name, val);
	if (cfg_set_gbl(name, val)) {
		CFG.saved_global = 1;
	}
}

// save current state
bool CFG_Save_Global_Settings()
{
	extern s32 wbfsDev;
	extern int grid_rows;
	extern int gui_style;
	CFG.saved_global = 1;
	STRCOPY(CFG.saved_theme, CFG.theme);
	CFG.saved_device = wbfsDev;
	CFG.saved_gui_rows = grid_rows;
	CFG.saved_gui_style = gui_style;
	if (gui_style == GUI_STYLE_COVERFLOW) {
		CFG.saved_gui_style += CFG_cf_global.theme;
	}
	STRCOPY(CFG.saved_partition, CFG.partition);
	CFG.saved_profile = CFG.current_profile;
	return CFG_Save_Settings();
}


bool CFG_Load_Settings()
{
	char pathname[200];
	bool ret;
	int i;
	snprintf(pathname, sizeof(pathname), "%s/%s", USBLOADER_PATH, "settings.cfg");
	// reset
	CFG.num_favorite_game = 0;
	CFG.saved_global = 0;
	CFG.current_profile = 0;
	profile_tag_index = 0;
	memset(&CFG.profile_favorite, 0, sizeof(CFG.profile_favorite));
	memset(&CFG.profile_num_favorite, 0, sizeof(CFG.profile_num_favorite));
	// load settings
	ret = cfg_parsefile(pathname, &settings_set);
	// set current profile
	i = cfg_find_profile(CFG.current_profile_name);
	CFG.current_profile = i >= 0 ? i : 0;
	// always hide uloader's cfg entry
	i = find_in_game_list((u8*)"__CF", CFG.hide_game, CFG.num_hide_game);
	if (i < 0)
		add_to_list((u8*)"__CF", CFG.hide_game, &CFG.num_hide_game, MAX_HIDE_GAME);
	// remember saved values
	STRCOPY(CFG.saved_theme, CFG.theme);
	CFG.saved_device    = CFG.device;
	CFG.saved_gui_style = CFG.gui_style;
	CFG.saved_gui_rows  = CFG.gui_rows;
	STRCOPY(CFG.saved_partition, CFG.partition);
	CFG.saved_profile   = CFG.current_profile;
	return ret;
}

bool CFG_Save_Settings()
{
	FILE *f;
	int i, j;
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
	fprintf(f, "# CFG USB Loader settings file %s\n", CFG_VERSION);
	fprintf(f, "# Note: This file is automatically generated\n");

	fprintf(f, "\n# Global Settings:\n");
	if (CFG.saved_global) {
		if (*CFG.saved_theme) {
			fprintf(f, "theme = %s\n", CFG.saved_theme);
		}
		char *dev_str[] = { "ask", "usb", "sdhc" };
		if (CFG.saved_device >= 0 && CFG.saved_device <= 2) {
			fprintf(f, "device = %s\n", dev_str[CFG.saved_device]);
		}
		fprintf(f, "partition = %s\n", CFG.saved_partition);
		char *s = map_get_name(map_gui_style, CFG.saved_gui_style);
		if (s) {
			fprintf(f, "gui_style = %s\n", s);
		}
		if (CFG.saved_gui_rows > 0 && CFG.saved_gui_rows <= 4 ) {
			fprintf(f, "gui_rows = %d\n", CFG.saved_gui_rows);
		}
	}

	fprintf(f, "\n# Profiles: %d\n", CFG.num_profiles);
	int save_prof = CFG.current_profile;
	for (j=0; j<CFG.num_profiles; j++) {
		CFG.current_profile = j;
		fprintf(f, "[profile=%s]\n", CFG.profile_names[j]);
		//fprintf(f, "# Profile: [%d] %s\n", j+1, CFG.profile_names[j]);
		fprintf(f, "# Favorite Games: %d\n", CFG.num_favorite_game);
		for (i=0; i<CFG.num_favorite_game; i++) {
			fprintf(f, "favorite_game = %.4s\n", CFG.favorite_game[i]);
		}
	}
	CFG.current_profile = save_prof;
	fprintf(f, "# Current profile: %d\n", CFG.saved_profile + 1);
	fprintf(f, "profile = %s\n", CFG.profile_names[CFG.saved_profile]);

	fprintf(f, "\n# Hidden Games: %d\n", CFG.num_hide_game);
	for (i=0; i<CFG.num_hide_game; i++) {
		fprintf(f, "hide_game = %.4s\n", CFG.hide_game[i]);
	}

	int saved_cfg_game = 0;
	for (i=0; i<num_cfg_game; i++) {
		if (cfg_game[i].is_saved) saved_cfg_game++;
	}
	fprintf(f, "\n# Game Options: %d\n", saved_cfg_game);
	for (i=0; i<num_cfg_game; i++) {
		if (!cfg_game[i].is_saved) continue;
		struct Game_CFG *game_cfg = &cfg_game[i].save;
		char *s;
		fprintf(f, "game:%s = ", cfg_game[i].id);
		#define SAVE_STR(N, S) \
			if (S) fprintf(f, "%s:%s; ", N, S)
		#define SAVE_BOOL(N) \
			fprintf(f, "%s:%d; ", #N, game_cfg->N)
		s = map_get_name(map_language, game_cfg->language);
		SAVE_STR("language", s);
		s = map_get_name(map_video, game_cfg->video);
		SAVE_STR("video", s);
		if (game_cfg->video_patch < 2) {
			SAVE_BOOL(video_patch);
		} else {
			SAVE_STR("video_patch", "all");
		}
		SAVE_BOOL(vidtv);
		SAVE_BOOL(country_patch);
		SAVE_BOOL(fix_002);
		s = ios_str(game_cfg->ios_idx);
		SAVE_STR("ios", s);
		SAVE_BOOL(block_ios_reload);
		if (game_cfg->alt_dol < 2) {
			SAVE_BOOL(alt_dol);
		} else {
			SAVE_STR("alt_dol", "disc");
			if (*game_cfg->dol_name) {
				SAVE_STR("dol_name", game_cfg->dol_name);
			}
		}
		SAVE_BOOL(ocarina);
		fprintf(f, "\n");
	}
	fprintf(f, "# END\n");
	fclose(f);
	return true;
}


// PER-GAME options

// find game cfg
struct Game_CFG_2* CFG_find_game(u8 *id)
{
	int i;
	for (i=0; i<num_cfg_game; i++) {
		if (memcmp(id, cfg_game[i].id, 6) == 0) {
			return &cfg_game[i];
		}
	}
	return NULL;
}

// find game cfg by id
struct Game_CFG CFG_read_active_game_setting(u8 *id)
{
	struct Game_CFG_2 *game = CFG_find_game(id);
	if (game != NULL)
		return game->curr;
	else
		return CFG.game;
}

// current options to game
void cfg_init_game(struct Game_CFG_2 *game, u8 *id)
{
	memset(game, 0, sizeof(*game));
	game->curr = CFG.game;
	game->save = CFG.game;
	memcpy((char*)game->id, (char*)id, 6);
	game->id[6] = 0;
	game->is_saved = 0;
}

// return existing or new
struct Game_CFG_2* CFG_get_game(u8 *id)
{
	struct Game_CFG_2 *game = CFG_find_game(id);
	if (game) return game;
	if (num_cfg_game >= MAX_CFG_GAME) return NULL;
	game = &cfg_game[num_cfg_game];
	num_cfg_game++;
	cfg_init_game(game, id);
	return game;
}

// free game opt slot
void cfg_free_game(struct Game_CFG_2* game)
{
	int i;
	if (num_cfg_game == 0 || !game) return;
	// move entries down
	num_cfg_game--;
	for (i=game-cfg_game; i<num_cfg_game; i++) {
		cfg_game[i] = cfg_game[i+1];
	}
	memset(&cfg_game[num_cfg_game], 0, sizeof(struct Game_CFG_2));
}

void game_set(char *name, char *val)
{
	// sample line:
	// game:RTNP41 = video:game; language:english; ocarina:0;
	// game:RYWP01 = video:patch; language:console; ocarina:1;
	//printf("GAME: '%s=%s'\n", name, val);
	u8 id[8];
	struct Game_CFG_2 *game;
	if (strncmp(name, "game:", 5) != 0) return;
	trimcopy((char*)id, name+5, sizeof(id)); 
	if (strlen((char*)id) != 6) return;
	game = CFG_get_game(id);
	// set id and current options as default
	cfg_init_game(game, id);
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
			cfg_set_game(opt_name, opt_val, &game->save);
		}
	}
	game->curr = game->save;
	game->is_saved = 1;
}

bool CFG_is_saved(u8 *id)
{
	struct Game_CFG_2 *game;
	game = CFG_find_game(id);
	if (game == NULL) return false;
	//return true;
	return game->is_saved;
}

// clean up so it's suitable for binary compare
void cfg_game_clean(struct Game_CFG *src, struct Game_CFG *dest)
{
	*dest = *src;
	memset(dest->dol_name, 0, sizeof(dest->dol_name));
	STRCOPY(dest->dol_name, src->dol_name);
	if (dest->alt_dol > 2) dest->alt_dol = 2;
}

// return true if differ
bool cfg_game_differ(struct Game_CFG *g1, struct Game_CFG *g2)
{
	// first make clean
	struct Game_CFG gg1, gg2;
	cfg_game_clean(g1, &gg1);
	cfg_game_clean(g2, &gg2);
	return memcmp(&gg1, &gg2, sizeof(gg1));
}

bool cfg_game_changed(struct Game_CFG_2 *game)
{
	if (game->is_saved) {
		// compare curr vs saved
		return cfg_game_differ(&game->curr, &game->save);
	}
	// curr vs global
	return cfg_game_differ(&game->curr, &CFG.game);
}

bool CFG_is_changed(u8 *id)
{
	struct Game_CFG_2 *game = CFG_find_game(id);
	if (!game) return false;
	return cfg_game_changed(game);
}

bool CFG_save_game_opt(u8 *id)
{
	struct Game_CFG_2 *game = CFG_get_game(id);
	if (!game) return false;
	game->save = game->curr;
	game->is_saved = 1;
	if (CFG_Save_Settings()) return true;
	return false;
}

bool CFG_discard_game_opt(u8 *id)
{
	struct Game_CFG_2 *game = CFG_find_game(id);
	if (!game) return false;
	// reset options
	cfg_init_game(game, id);
	return CFG_Save_Settings();
}

void CFG_release_game(struct Game_CFG_2 *game)
{
	if (game->is_saved) return;
	if (cfg_game_changed(game)) return;
	// not saved and not changed - free slot
	cfg_free_game(game);
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

	if (!*theme) return;

	snprintf(D_S(theme_path), "%s/themes/%s", USBLOADER_PATH, theme);
	snprintf(D_S(pathname), "%s/theme.txt", theme_path);
	if (stat(pathname, &st) == 0) {
		CFG_Default_Theme();
		STRCOPY(CFG.theme, theme);
		copy_theme_path(CFG.background, "background.png", sizeof(CFG.background));
		if (!copy_theme_path(CFG.w_background, "background_wide.png",
					sizeof(CFG.w_background)))
		{
			// if not found use normal
			*CFG.w_background = 0;
		}
		copy_theme_path(CFG.bg_gui, "bg_gui.png", sizeof(CFG.bg_gui));
		if (!copy_theme_path(CFG.bg_gui_wide, "bg_gui_wide.png",
					sizeof(CFG.bg_gui_wide)))
		{
			// if themed bg_gui_wide.png not found, use themed bg_gui.png
			// instead of global bg_gui_wide.png
			*CFG.bg_gui_wide = 0;
		}
		cfg_parsefile(pathname, &theme_set);
		// override some theme options with config.txt:
		snprintf(D_S(pathname), "%s/config.txt", USBLOADER_PATH);
		cfg_parsefile(pathname, &theme_set_base);
		STRCOPY(CFG.theme_path, theme_path);
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

char *get_clock_str(time_t t)
{
	static char clock_str[16];
	char *fmt = "%H:%M";
	if (CFG.clock_style == 12) fmt = "%I:%M";
	if (CFG.clock_style == 13) fmt = "%I:%M %p";
	strftime(clock_str, sizeof(clock_str), fmt, localtime(&t));
	return clock_str;
}

//extern u8 __text_start[];
//extern u8 __Arena1Lo[], __Arena1Hi[];
//extern u8 __Arena2Lo[], __Arena2Hi[];
void test_unicode()
{
	wchar_t wc;
	char mb[32*6+1];
	int i, k, len=0, count=0;
	printf("\n");
	for (i=0; i<512; i++) {
		wc = i;
		switch (wc) {
			case '\n': strcat(mb, "n"); k=1; break;
			case '\r': strcat(mb, "r"); k=1; break;
			case '\b': strcat(mb, "b"); k=1; break;
			case '\f': strcat(mb, "f"); k=1; break;
			case '\t': strcat(mb, "t"); k=1; break;
			case 0x1b: strcat(mb, "e"); k=1; break; // esc
			default:
			k = wctomb(mb+len, wc);
			if (k < 1 || mb[len] == 0) {
				if (wc < 128) {
					mb[len] = '!'; //wc;
				} else {
					mb[len] = '?';
				}
				k = 1;
			}
		}
		len += k;
		mb[len] = 0;
		count++;
		if (count >= 32) {
			printf("%03x: %s |\n", (unsigned)i-31, mb);
			len = 0;
			count = 0;
		}
	}
	printf("\n    Press any button to continue...");
	Wpad_WaitButtonsCommon();
	printf("\n\n");
}

/*
debug codes:
1 : basic
2 : unicode
3 : coverflow
4 : banner sound
8 : io benchmark
*/

void cfg_debug(int argc, char **argv)
{
	if (!CFG.debug) return;

	Gui_Console_Enable();
	__console_scroll = 1;
	printf("base_path: %s\n", USBLOADER_PATH);
	printf("apps_path: %s\n", APPS_DIR);
	printf("bg_path: %s\n", *CFG.background ? CFG.background : "builtin");
	printf("covers_path: %s\n", CFG.covers_path);
	printf("theme_path: %s\n", CFG.theme_path);
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
	extern char *get_cc();
	printf("CC: %s ", get_cc());
	printf("\n");
	printf("url: %s\n", CFG.cover_url_2d_norm);
	//printf("t[%.6s]=%s\n", cfg_title[0].id, cfg_title[0].title);
	//printf("hide_hdd: %d\n", CFG.hide_hddinfo);
	//printf("text start: %p\n", __text_start);
	//printf("M1: %p - %p\n", __Arena1Lo, __Arena1Hi);
	//printf("M2: %p - %p\n", __Arena2Lo, __Arena2Hi);
	int i;
	for (i=0; i<argc; i++) {
		printf("arg[%d]: %s ", i, argv[i]);
	}
	
	printf("\n");
	printf("# Hidden Games: %d\n", CFG.num_hide_game);
	for (i=0; i<CFG.num_hide_game; i++) {
		printf("%.4s ", CFG.hide_game[i]);
	}

	sleep(1);
	printf("\n    Press any button to continue...");
	Wpad_WaitButtonsCommon();
	if (CFG.debug & 2) test_unicode();
	printf("\n\n");
}

void chdir_app(char *arg)
{
	char dir[200], *pos1, *pos2;
	struct stat st;
	if (arg == NULL) goto default_dir;
	// check if starts with sd: or usb:
	if (strncmp(arg, "sd:/", 4) == 0
		|| strncmp(arg, "usb:/", 5) == 0)
	{
		// trim /boot.dol
		STRCOPY(APPS_DIR, arg);
		pos1 = strrchr(APPS_DIR, '/');
		if (!pos1) goto default_dir;
		*pos1 = 0;
		// still a valid path?
		pos1 = strchr(APPS_DIR, '/');
		if (!pos1) goto default_dir;
	} else {
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
	// set current profile
	int i = cfg_find_profile(CFG.current_profile_name);
	CFG.current_profile = i >= 0 ? i : 0;
}

// before video & console init
void cfg_setup2()
{
	// setup cover style: path & url
	cfg_setup_cover_style();

	// setup cover dimensions
	CFG.N_COVER_WIDTH = COVER_WIDTH;
	CFG.N_COVER_HEIGHT = COVER_HEIGHT;
	
	// setup widescreen coords
	if (CFG.widescreen) {
		// copy over
		CONSOLE_XCOORD = CFG.W_CONSOLE_XCOORD;
		CONSOLE_YCOORD = CFG.W_CONSOLE_YCOORD;
		CONSOLE_WIDTH  = CFG.W_CONSOLE_WIDTH;
		CONSOLE_HEIGHT = CFG.W_CONSOLE_HEIGHT;
		COVER_XCOORD   = CFG.W_COVER_XCOORD;
		COVER_YCOORD   = CFG.W_COVER_YCOORD;
		if (*CFG.w_background) {
			STRCOPY(CFG.background, CFG.w_background);
		}
		if (!CFG.W_COVER_WIDTH || !CFG.W_COVER_HEIGHT) {
			// if either wide val is 0: automatic resize
			// normal (4:3): COVER_WIDTH = 160;
			// wide (16:9): COVER_WIDTH = 130;
			// ratio: *13/16
			// although true 4:3 -> 16:9 should be *12/16,
			// but it looks too compressed
			// and align to multiple of 2
			CFG.W_COVER_WIDTH = (COVER_WIDTH * 13 / 16) / 2 * 2;
			// height is same
			CFG.W_COVER_HEIGHT = COVER_HEIGHT;
		}
		COVER_WIDTH = CFG.W_COVER_WIDTH;
		COVER_HEIGHT = CFG.W_COVER_HEIGHT;
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
	if (CFG.console_mark_saved) MAX_CHARACTERS--;

	ENTRIES_PER_PAGE = rows - 5;
	// correct ENTRIES_PER_PAGE
	if (CFG.db_show_info) {  // lustar
		ENTRIES_PER_PAGE -= 2;
	}
	if (CFG.hide_header) ENTRIES_PER_PAGE++;
	if (CFG.hide_hddinfo) ENTRIES_PER_PAGE++;
	if (CFG.buttons == CFG_BTN_OPTIONS_1 || CFG.buttons == CFG_BTN_OPTIONS_B) {
		if (CFG.hide_footer) ENTRIES_PER_PAGE++;
	} else {
		ENTRIES_PER_PAGE--;
		if (CFG.hide_footer) ENTRIES_PER_PAGE+=2;
	}
}


void cfg_direct_start(int argc, char **argv)
{
	// loadstructor
	// patched buffer in binary
	char *direct_start_arg = NULL;
	//if (strcmp(direct_start_id_buf, "#GAMEID") != 0)
	// we can't compare "#GAMEID" directly because
	// it has to be a unique string inside the binary
	char *p = direct_start_id_buf;
	if (p[0] != '#' ||
		p[1] != 'G' ||
		p[2] != 'A' ||
		p[3] != 'M' ||
		p[4] != 'E' ||
		p[5] != 'I' ||
		p[6] != 'D' )
	{
		direct_start_arg = direct_start_id_buf + 1;
	}
	// uloader forwarder arg variant
	// argv[1] = "#RHAP01" or "#RHAP01-2"
	if(argc>1 && argv && argv[1]) {
		if(argv[1][0] == '#') {
			direct_start_arg = argv[1] + 1;
		} else {
			extern bool is_gameid(char *id);
			if ((strlen(argv[1]) == 6) && is_gameid(argv[1])) {
				direct_start_arg = argv[1];
			}
		}
	}
	// is direct start?
	if (direct_start_arg) {
		char *t = direct_start_arg;
		// game id
		memcpy(CFG.launch_discid, t, 6);
		t += 6;
		if (t[0] == '-') {
			int part_num = t[1] - '0';
			//CFG.current_partition = t[1] - 48;
			//CFG.current_partition &= 3;
			if (part_num < 4) {
				sprintf(CFG.partition, "WBFS%d", part_num + 1);
			} else {
				sprintf(CFG.partition, "FAT%d", part_num - 4 + 1);
			}
		}
		// setup cfg for auto-start
		CFG.direct_launch = 1;
		CFG_Default_Theme();
		CFG.device = CFG_DEV_USB;
		*CFG.background = 0;
		*CFG.w_background = 0;
		CFG.covers = 0;
		CFG.gui = 0;
		CFG.music = 0;
		CFG.confirm_start = 0;
		//CFG.debug = 0;
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

	// are we started from usb? then check usb first.
	// also if wiiload supplied first argument is "usb:", start with usb.
	if (argc && argv && argv[0]) {
		if (strncmp(argv[0], "usb:", 4) == 0
			|| (argc>1 && strcmp(argv[1], "usb:") == 0) )
		{
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
	STRCOPY(LAST_CFG_PATH, USBLOADER_PATH);

	// load renamed titles
	snprintf(filename, sizeof(filename), "%s/%s", USBLOADER_PATH, "titles.txt");
	cfg_parsefile(filename, &title_set);

	// load per-app local config and titles
	// (/apps/USBLoader/config.txt and titles.txt if run from HBC)
	if (strcmp(USBLOADER_PATH, APPS_DIR) != 0) {
		if (cfg_parsefile("config.txt", &cfg_set)) {
			STRCOPY(LAST_CFG_PATH, APPS_DIR);
		}
		cfg_parsefile("titles.txt", &title_set);
	}

	// load per-game settings
	CFG_Load_Settings();

	// loadstructor direct start support
	cfg_direct_start(argc, argv);

	// parse commandline arguments (wiiload)
	cfg_parsearg(argc, argv);

	// get theme list
	if (!CFG.direct_launch) {
		get_theme_list();
	}

	// setup dependant and calculated parameters
	cfg_setup1();
	cfg_setup2();
	
	// set coverflow defaults
	CFG_Default_Coverflow();
	CFG_Default_Coverflow_Themes();
}


void CFG_Setup(int argc, char **argv)
{
	cfg_setup3();
	// load database
	ReloadXMLDatabase(USBLOADER_PATH, CFG.db_language, 1);
	if (!playStatsRead(0)) {
		if (readPlayStats() > -1) playStatsRead(1);
	}
	cfg_debug(argc, argv);
}

u32 getPlayCount(u8 *id) {
	int n = 0;
	for (;n<playStatsSize; n++) {
		if (!strcmp(playStats[n].id, (char *)id))
			return playStats[n].playCount;
	}
	return 0;
}

time_t getLastPlay(u8 *id) {
	int n = 0;
	for (;n<playStatsSize; n++) {
		if (!strcmp(playStats[n].id, (char *)id))
			return playStats[n].playTime;
	}
	return 0;
}

bool playStatsRead(int i) { //i < 0 checks read without modifying it
	static bool read = 0;
	if (i > 0) read = 1;
	if (i == 0) read = 0;
	return read;
}

int readPlayStats() {
	int n = 0;
	char filepath[MAXPATHLEN];
	snprintf(filepath, sizeof(filepath), "%s/playstats.txt", USBLOADER_PATH);
	time_t start;
	int count;
	char id[7];
	FILE *f;
	f = fopen(filepath, "r");
	if (!f) {
		return -1;
	}
	while (fscanf(f, "%6s:%d:%ld\n", id, &count, &start) == 3) {
		if (n >= playStatsSize) {
			playStatsSize++;
			playStats = (struct playStat*)mem1_realloc(playStats, (playStatsSize * sizeof(struct playStat)));
			if (!playStats)
				return -1;
		}
		strncpy(playStats[n].id, id, 7);
		playStats[n].playCount = count;
		playStats[n].playTime = start;
		n++;
	}
	fclose(f);
	return 0;
}

int setPlayStat(u8 *id) {
	if (CFG.write_playstats == 0)
		return 0;
	char filepath[MAXPATHLEN];
	time_t now;
	int n = 0;
	int count = 0;
	snprintf(filepath, sizeof(filepath), "%s/playstats.txt", USBLOADER_PATH);
	FILE *f;

	if (!playStatsRead(0)) {
		if (readPlayStats() > -1) playStatsRead(1);
	}

	f = fopen(filepath, "r+");
	if (!f) {
		f = fopen(filepath, "w");
		if (!f) {
			return -1;
		}
		now = time(NULL);
		fprintf(f, "%s:%d:%ld\n", (char *)id, 1, now);
		fclose(f);
		return 1;
	}
	for (n=0; n<playStatsSize; n++) {
		if (strcmp(playStats[n].id, (char *)id)) {
			fprintf(f, "%s:%d:%ld\n", playStats[n].id, playStats[n].playCount, playStats[n].playTime);
		} else {
			count = playStats[n].playCount;
		}
	}
	now = time(NULL);
	fprintf(f, "%s:%d:%ld\n", (char *)id, count+1, now);
	fclose(f);
	return 1;
}

#ifdef FAKE_GAME_LIST

#include <malloc.h>
#include "wbfs.h"

// initial/max game list size
int fake_games = 2300;
//int fake_games = 0;

// current game list size
int fake_num = 0;

struct discHdr *fake_list = NULL;

int is_fake(char *id)
{
	int i;
	for (i=0; i<fake_num; i++) {
		// ignore region, check only first 5
		if (strncmp((char*)fake_list[i].id, (char*)id, 5) == 0) return 1;
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
	SAFE_FREE(fake_list);
	if (fake_games < 0) fake_games = 0;

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
	if (fake_num > fake_games) fake_num = fake_games;

	//printf("fake dir: %s\n", CFG.covers_path); sleep(1);
	dir = opendir(CFG.covers_path);
	if (!dir) {
		printf("fake dir error! %s\n", CFG.covers_path); sleep(2);
		return 0;
	}

	while ((dent = readdir(dir)) != NULL) {
		if (fake_num >= fake_games) break;
		if (dent->d_name[0] == '.') continue;
		if (strstr(dent->d_name, ".png") == NULL
			&& strstr(dent->d_name, ".PNG") == NULL) continue;
		memset(id, 0, sizeof(id));
		STRCOPY(id, dent->d_name);
		p = strchr(id, '.');
		if (p == NULL) continue;
		*p = 0;
		// check if already exists, do not ignore region, we want more games ;)
		if (is_fake(id)) continue;
		fake_list = realloc(fake_list, sizeof(struct discHdr) * (fake_num+1));
		memset(fake_list+fake_num, 0, sizeof(struct discHdr));
		memcpy(fake_list[fake_num].id, id, sizeof(fake_list[fake_num].id));
		STRCOPY(fake_list[fake_num].title, dent->d_name);
		//printf("fake %d %.6s %s\n", fake_num,
		//		fake_list[fake_num].id, fake_list[fake_num].title);
		fake_num++;

	}
	closedir(dir);
	//sleep(2);
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

