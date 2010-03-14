
// Modified by oggzee & usptactical

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <ctype.h>
#include <wctype.h>
#include <wchar.h>
#include <locale.h>
#include <asndlib.h>

#include "disc.h"
#include "fat.h"
#include "cache.h"
#include "gui.h"
#include "menu.h"
#include "partition.h"
#include "restart.h"
#include "sys.h"
#include "util.h"
#include "utils.h"
#include "video.h"
#include "wbfs.h"
#include "libwbfs/libwbfs.h"
#include "wpad.h"
#include "patchcode.h"
#include "cfg.h"
#include "http.h"
#include "dns.h"
#include "wdvd.h"
#include "music.h"
#include "subsystem.h"
#include "net.h"
#include "fst.h"
#include "wiip.h"
#include "frag.h"
#include "xml.h" /* WiiTDB database - Lustar */
#include "sort.h"
#include "gettext.h"
#include "playlog.h"

#define CHANGE(V,M) {V+=change; if(V>(M)) V=(M); if(V<0) V=0;}


char CFG_VERSION[] = CFG_VERSION_STR;

void Sys_Exit();

extern int gui_style;
extern long long gettime();
extern u32 diff_msec(long long start,long long end);
extern int __console_disable;

/* Gamelist buffer */
struct discHdr *all_gameList = NULL;
static struct discHdr *fav_gameList = NULL;
struct discHdr *gameList = NULL;
struct discHdr *filter_gameList = NULL;

/* Gamelist variables */
bool enable_favorite = false;
s32 all_gameCnt = 0;
s32 fav_gameCnt = 0;
s32 filter_gameCnt = 0;
extern s32 filter_type;
extern s32 filter_index;

s32 gameCnt = 0, gameSelected = 0, gameStart = 0;

bool imageNotFound = false;

/* admin unlock mode variables */
static int disable_format = 0;
static int disable_remove = 0;
static int disable_install = 0;
static int disable_options = 0;
static int confirm_start = 1;
static bool unlock_init = true;

/*VIDEO OPTION - hungyip84 */
char videos[CFG_VIDEO_MAX+1][15] = 
{
{"System Def."},
{"Game Default"},
{"Force PAL50"},
{"Force PAL60"},
{"Force NTSC"}
//{"Patch Game"},
};

/*LANGUAGE PATCH - FISHEARS*/
char languages[11][22] =
{
{"Console Def."},
{"Japanese"},
{"English"},
{"German"},
{"French"},
{"Spanish"},
{"Italian"},
{"Dutch"},
{"S. Chinese"},
{"T. Chinese"},
{"Korean"}
};
/*LANGUAGE PATCH - FISHEARS*/

int Menu_Global_Options();
int Menu_Game_Options();
void Switch_Favorites(bool enable);
void bench_io();

char action_string[40] = "";

#ifdef FAKE_GAME_LIST
// Debug Test mode with fake game list
#define WBFS_GetCount dbg_WBFS_GetCount
#define WBFS_GetHeaders dbg_WBFS_GetHeaders
#endif

char *skip_sort_ignore(char *s)
{
	char tok[200], *p;
	int len;
	p = CFG.sort_ignore;
	while (p) {
		p = split_token(tok, p, ',', sizeof(tok));
		len = strlen(tok);
		if (len && strncasecmp(s, tok, strlen(tok)) == 0) {
			if (s[len] == ' ' || s[len] == '\'') {
				s += len + 1;
			}
		}
	}
	return s;
}

s32 __Menu_GetEntries(void)
{
	struct discHdr *buffer = NULL;

	u32 cnt, len;
	s32 ret;

	Cache_Invalidate();

	Switch_Favorites(false);
	SAFE_FREE(fav_gameList);
	fav_gameCnt = 0;
	
	
	SAFE_FREE(filter_gameList);
	filter_gameCnt = 0;

	/* Get list length */
	ret = WBFS_GetCount(&cnt);
	if (ret < 0)
		return ret;

	/* Buffer length */
	len = sizeof(struct discHdr) * cnt;

	/* Allocate memory */
	buffer = (struct discHdr *)memalign(32, len);
	if (!buffer)
		return -1;

	/* Clear buffer */
	memset(buffer, 0, len);

	/* Get header list */
	ret = WBFS_GetHeaders(buffer, cnt, sizeof(struct discHdr));
	if (ret < 0)
		goto err;

	/* Sort entries */
	__set_default_sort();
	qsort(buffer, cnt, sizeof(struct discHdr), default_sort_function);

	// hide and re-sort preffered
	if (!CFG.admin_lock || CFG.admin_mode_locked)
		cnt = CFG_hide_games(buffer, cnt);
	CFG_sort_pref(buffer, cnt);

	/* Free memory */
	if (gameList)
		free(gameList);

	/* Set values */
	gameList = buffer;
	gameCnt  = cnt;

	/* Reset variables */
	gameSelected = gameStart = 0;

	// init favorites
	all_gameList = gameList;
	all_gameCnt  = gameCnt;
	len = sizeof(struct discHdr) * all_gameCnt;
	fav_gameList = (struct discHdr *)memalign(32, len);
	if (fav_gameList) {
		memcpy(fav_gameList, all_gameList, len);
		fav_gameCnt = all_gameCnt;
	}
	
	filter_gameList = (struct discHdr *)memalign(32, len);
	if (filter_gameList) {
		memcpy(filter_gameList, all_gameList, len);
		filter_gameCnt = all_gameCnt;
	}

	return 0;

err:
	/* Free memory */
	if (buffer)
		free(buffer);

	return ret;
}

void Switch_Favorites(bool enable)
{
	int i, len;
	u8 *id = NULL;
	// filter
	if (fav_gameList) {
		len = sizeof(struct discHdr) * all_gameCnt;
		memcpy(fav_gameList, all_gameList, len);
		fav_gameCnt = CFG_filter_favorite(fav_gameList, all_gameCnt);
	}
	// switch
	//printf("fav %d %p %d\n", enable, fav_gameList, fav_gameCnt); sleep(5);
	if (fav_gameList == NULL || fav_gameCnt == 0) enable = false;
	if (gameSelected < gameCnt) {
		id = gameList[gameSelected].id;
	}
	if (enable) {
		gameList = fav_gameList;
		gameCnt = fav_gameCnt;
	} else {
		gameList = all_gameList;
		gameCnt = all_gameCnt;
	}
	enable_favorite = enable;
	// find game selected
	gameStart = 0;
	gameSelected = 0;
	for (i=0; i<gameCnt; i++) {
		if (strncmp((char*)gameList[i].id, (char*)id, 6) == 0) {
			gameSelected = i;
			break;
		}
	}
	// scroll start list
	__Menu_ScrollStartList();
}

char *__Menu_PrintTitle(char *name)
{
	//static char buffer[MAX_CHARACTERS + 4];
	static char buffer[200];
	int len = con_len(name);

	/* Clear buffer */
	memset(buffer, 0, sizeof(buffer));

	/* Check string length */
	if (len >= MAX_CHARACTERS) {
		//strncpy(buffer, name,  MAX_CHARACTERS - 4);
		STRCOPY(buffer, name);
		con_trunc(buffer, MAX_CHARACTERS - 4);
		strcat(buffer, "...");
		return buffer;
	}

	return name;
}

bool is_dual_layer(u64 real_size)
{
	//return true; // dbg
	u64 single = 4699979776LL;
	if (real_size > single) return true;
	return false;
}

int get_game_ios_idx(struct Game_CFG_2 *game_cfg)
{
	if (game_cfg == NULL) {
		return CURR_IOS_IDX;
	}
	return game_cfg->curr.ios_idx;
}

void warn_ios_bugs()
{
	bool warn = false;
	if (!is_ios_type(IOS_TYPE_WANIN)) {
		CFG.patch_dvd_check = 1;
		return;
	}
	// ios == 249
	if (wbfs_part_fs && IOS_GetRevision() < 18) {
		printf(
			gt("ERROR: CIOS222 or CIOS223 required for\n"
			"starting games from a FAT partition!"));
		printf("\n\n");
		warn = true;
	}
	if (IOS_GetRevision() < 14) {
		printf(
			gt("NOTE: CIOS249 before rev14:\n"
			"Possible error #001 is not handled!"));
		printf("\n\n");
		warn = true;
	}
	if (IOS_GetRevision() == 13) {
		printf(
			gt("NOTE: You are using CIOS249 rev13:\n"
			"To quit the game you must reset or\n"
			"power-off the Wii before you start\n"
			"another game or it will hang here!"));
		printf("\n\n");
		warn = true;
	}
	if (IOS_GetRevision() < 12) {
		CFG.patch_dvd_check = 1;
		/*printf(
			"NOTE: CIOS249 before rev12:\n"
			"Any disc in the DVD drive is required!\n\n");
		warn = true;*/
	}
	if (IOS_GetRevision() < 10
			&& wbfsDev == WBFS_DEVICE_SDHC) {
		printf(
			gt("NOTE: CIOS249 before rev10:\n"
			"Loading games from SDHC not supported!"));
		printf("\n\n");
		warn = true;
	}
	if (IOS_GetRevision() < 9
			&& wbfsDev == WBFS_DEVICE_USB) {
		printf(
			gt("NOTE: CIOS249 before rev9:\n"
			"Loading games from USB not supported!"));
		printf("\n\n");
		warn = true;
	}
	if (warn) sleep(1);
}

void print_dual_layer_note()
{
	printf(
		gt("NOTE: You are using CIOS249 rev14:\n"
		"It has known problems with dual layer\n"
		"games. It is highly recommended that\n"
		"you use a different IOS or revision\n"
		"for installation/playback of this game."));
	printf("\n\n");
}

bool check_device(struct Game_CFG_2 *game_cfg, bool print)
{
	int ii = get_game_ios_idx(game_cfg);
	/*
	if (wbfsDev == WBFS_DEVICE_SDHC) {
		if (ii != CFG_IOS_249 && ii != CFG_IOS_250) {
			if (print) printf(
				"ERROR: loading games from SDHC\n"
				"supported only with IOS249\n\n");
			return false;
		}
	}
	*/
	if (wbfs_part_idx > 1) {
		if (!is_ios_idx_mload(ii))
		{
			if (print) {
				printf(
					gt("ERROR: multiple wbfs partitions\n"
					"supported only with IOS222/223-mload"));
				printf("\n\n");
			}
			return false;
		}
	}
	return true;
}

bool check_dual_layer(u64 real_size, struct Game_CFG_2 *game_cfg)
{
	//return true; // dbg
	if (!is_dual_layer(real_size)) return false;
	if (game_cfg == NULL) {
		if (is_ios_type(IOS_TYPE_WANIN) && (IOS_GetRevision() == 14)) return true;
		return false;
	}
	if (game_cfg->curr.ios_idx != CFG_IOS_249) return false; 
	if (is_ios_type(IOS_TYPE_WANIN)) {
	   	if (IOS_GetRevision() == 14) return true;
		return false;
	}
	// we are in 222 but cfg is 249, we can't know revision here
	// so we'll just return true
	return true;
}


void __Menu_PrintInfo2(struct discHdr *header, u64 comp_size, u64 real_size)
{
	float size = (float)comp_size / 1024 / 1024 / 1024;
	char *dl_str = is_dual_layer(real_size) ? "(dual-layer)" : "";

	/* Print game info */
	printf_("%s\n", get_title(header));
	printf_("(%.6s) ", header->id);
	printf("(%.2f%s) ", size, gt("GB"));
	printf("%s\n\n", dl_str);
	//printf(" comp: %lld (%.2fMB)\n", comp_size, (f32)comp_size/1024/1024);
	//printf(" real: %lld (%.2fMB)\n", real_size, (f32)real_size/1024/1024);
}

void __Menu_PrintInfo(struct discHdr *header)
{
	//f32 size = 0.0;
	u64 comp_size, real_size = 0;

	/* Get game size */
	//WBFS_GameSize(header->id, &size);
	WBFS_GameSize2(header->id, &comp_size, &real_size);

	/* Print game info */
	//printf_("%s\n", get_title(header));
	//printf_("(%.6s) (%.2fGB) %s\n\n", header->id, size, dl_str);
	__Menu_PrintInfo2(header, comp_size, real_size);
}

void __Menu_MoveList(s8 delta)
{
	/* No game list */
	if (!gameCnt)
		return;

	#if 0
	/* Select next entry */
	gameSelected += delta;

	/* Out of the list? */
	if (gameSelected <= -1)
		gameSelected = (gameCnt - 1);
	if (gameSelected >= gameCnt)
		gameSelected = 0;
	#endif

	if(delta>0) {
		if(gameSelected == gameCnt - 1) {
			gameSelected = 0;
		}
		else {
			gameSelected +=delta;
			if(gameSelected >= gameCnt) {
				gameSelected = (gameCnt - 1);
			}
		}
	}
	else {
		if(!gameSelected) {
			gameSelected = gameCnt - 1;
		}
		else {
			gameSelected +=delta;
			if(gameSelected < 0) {
				gameSelected = 0;
			}
		}
	}

	/* List scrolling */
	__Menu_ScrollStartList();
}

void __Menu_ScrollStartList()
{
	s32 index = (gameSelected - gameStart);

	if (index >= ENTRIES_PER_PAGE)
		gameStart += index - (ENTRIES_PER_PAGE - 1);
	if (index <= -1)
		gameStart += index;
}

void __Menu_ShowList(void)
{
	FgColor(CFG.color_header);
	if (enable_favorite) {
		printf_x(gt("Favorite Games"));
		printf(":\n");
	} else {
		if (!CFG.hide_header) {
			printf_x(gt("Select the game you want to boot"));
			printf(":\n");
		}
	}
	DefaultColor();
	if (CFG.console_mark_page && gameStart > 0) {
		printf(" %s +", CFG.cursor_space);
	}
	printf("\n");

	/* No game list*/
	if (gameCnt) {
		u32 cnt;

		/* Print game list */
		for (cnt = gameStart; cnt < gameCnt; cnt++) {
			struct discHdr *header = &gameList[cnt];

			/* Entries per page limit reached */
			if ((cnt - gameStart) >= ENTRIES_PER_PAGE)
				break;

			if (gameSelected == cnt) {
				FgColor(CFG.color_selected_fg);
				BgColor(CFG.color_selected_bg);
				Con_ClearLine();
			} else {
				DefaultColor();
			}

			/* Print entry */
			//printf(" %2s %s\n", (gameSelected == cnt) ? ">>" : "  ",
			char *title = __Menu_PrintTitle(get_title(header));
			// cursor
			printf(" %s", (gameSelected == cnt) ? CFG.cursor : CFG.cursor_space);
			// favorite mark
			printf("%s", (CFG.console_mark_favorite && is_favorite(header->id))
					? CFG.favorite : " ");
			// title
			printf("%s", title);
			// saved mark
			if (CFG.console_mark_saved) {
				printf("%*s", (MAX_CHARACTERS - con_len(title)),
					(CFG_is_saved(header->id)) ? CFG.saved : " ");
			}
			printf("\n");
		}
		DefaultColor();
		if (CFG.console_mark_page && cnt < gameCnt) {
			printf(" %s +", CFG.cursor_space);
		} else {
			printf(" %s  ", CFG.cursor_space);
		}
		//if (CFG.hide_hddinfo) {
		FgColor(CFG.color_footer);
		BgColor(CONSOLE_BG_COLOR);
		int num_page = 1 + (gameCnt - 1) / ENTRIES_PER_PAGE;
		int cur_page = 1 + gameSelected / ENTRIES_PER_PAGE;
		printf(" %-*.*s %d/%d", MAX_CHARACTERS - 8, MAX_CHARACTERS - 8, action_string, cur_page, num_page);
		if (!CFG.hide_hddinfo) printf("\n");
		action_string[0] = 0;
		//}
	} else {
		printf(" ");
		printf(gt("%s No games found!!"), CFG.cursor);
		printf("\n");
	}

	/* Print free/used space */
	FgColor(CFG.color_footer);
	BgColor(CONSOLE_BG_COLOR);
	if (!CFG.hide_hddinfo) {
		// Get free space
		f32 free, used;
		//printf("\n");
		WBFS_DiskSpace(&used, &free);
		printf_x(gt("Used: %.1fGB Free: %.1fGB [%d]"), used, free, gameCnt);
		//int num_page = 1 + (gameCnt - 1) / ENTRIES_PER_PAGE;
		//int cur_page = 1 + gameSelected / ENTRIES_PER_PAGE;
		//printf(" %d/%d", cur_page, num_page);
	}
	if (!CFG.hide_footer) {
		printf("\n");
		// (B) GUI (1) Options (2) Favorites
		// B: GUI 1: Options 2: Favorites
		//char c_gui = 'B', c_opt = '1';
		//if (CFG.buttons == CFG_BTN_OPTIONS_B) {
		//	c_gui = '1'; c_opt = 'B';
		//}
		printf_("");
		if (CFG.gui && CFG.button_gui) {
			printf("%s: GUI ", (button_names[CFG.button_gui]));
		}
		if (!CFG.disable_options && CFG.button_opt) {
			printf("%s: Options ", (button_names[CFG.button_opt]));
		}
		if (CFG.button_fav) {
			printf("%s: Favorites", (button_names[CFG.button_fav]));
		}
	}
	if (CFG.db_show_info) {
		printf("\n");
		//load game info from XML - lustar
		__Menu_ShowGameInfo(false, gameList[gameSelected].id);
	}
	DefaultColor();
	__console_flush(0);
}

/* load game info from XML - lustar */
void __Menu_ShowGameInfo(bool showfullinfo, u8 *id)
{
	if (LoadGameInfoFromXML(id)) {
		FgColor(CFG.color_inactive);
		PrintGameInfo(showfullinfo);
		//printf("Play Count: %d\n", getPlayCount(id));
		DefaultColor();
	}
}

void __Menu_ShowCover(void)
{
	struct discHdr *header = &gameList[gameSelected];

	/* No game list*/
	if (!gameCnt)
		return;

	/* Draw cover */
	Gui_DrawCover(header->id);
}

bool Save_ScreenShot(char *fn, int size)
{
	int i;
	struct stat st;
	for (i=1; i<100; i++) {
		snprintf(fn, size, "%s/screenshot%d.png", USBLOADER_PATH, i);
		if (stat(fn, &st)) break;
	}
	return ScreenShot(fn);
}

void Make_ScreenShot()
{
	bool ret;
	char fn[200];
	ret = Save_ScreenShot(fn, sizeof(fn));
	printf("\n%s: %s\n", ret ? "Saved" : "Error Saving", fn);
	sleep(1);
}

void Handle_Home(int disable_screenshot)
{
	if (CFG.home == CFG_HOME_EXIT) {
		Con_Clear();
		printf("\n");
		printf_("Exiting...");
		__console_flush(0);
		Sys_Exit();
	} else if (CFG.home == CFG_HOME_SCRSHOT) {
		__console_flush(0);
		Make_ScreenShot();
		if (disable_screenshot)	CFG.home = CFG_HOME_EXIT;
	} else if (CFG.home == CFG_HOME_HBC) {
		Con_Clear();
		printf("\n");
		printf_("HBC...");
		__console_flush(0);
		Sys_HBC();
	} else if (CFG.home == CFG_HOME_REBOOT) { 
		Con_Clear();
		Restart();
	} else {
		// Priiloader magic words, and channels
		if ((CFG.home & 0xFF) < 'a') {
			// upper case final letter implies channel
			Con_Clear();
			Sys_Channel(CFG.home);
		} else {
			// lower case final letter implies magic word
			Con_Clear();
			*(vu32*)0x8132FFFB = CFG.home;
			Restart();
		}
	}
}

void Print_SYS_Info()
{
	extern int mload_ehc_fat;
	int new_wanin = is_ios_type(IOS_TYPE_WANIN) && IOS_GetRevision() >= 18;
	FgColor(CFG.color_inactive);
	printf_("");
	Fat_print_sd_mode();
	printf_(gt("CFG base: %s"), USBLOADER_PATH);
	printf("\n");
	if (strcmp(LAST_CFG_PATH, USBLOADER_PATH)) {
		// if last cfg differs, print it out
		printf_(gt("Additional config:"));
		printf("\n");
		printf_("  %s/config.txt\n", LAST_CFG_PATH);
	}
	printf_(gt("Loader Version: %s"), CFG_VERSION);
	printf("\n");
	printf_("IOS%u (Rev %u) %s\n",
			IOS_GetVersion(), IOS_GetRevision(),
			mload_ehc_fat||new_wanin ? "[FRAG]" : "");
	if (CFG.ios_mload || new_wanin) {
		printf_("");
		print_mload_version();
	}
	DefaultColor();
}

// WiiMote to char map for admin mode unlocking
char get_unlock_buttons(buttons)
{
	switch (buttons)
	{
		case WPAD_BUTTON_UP:	return 'U';
		case WPAD_BUTTON_DOWN:	return 'D';
		case WPAD_BUTTON_LEFT:	return 'L';
		case WPAD_BUTTON_RIGHT:	return 'R';
		case WPAD_BUTTON_A:	return 'A';
		case WPAD_BUTTON_B:	return 'B';
		case WPAD_BUTTON_MINUS:	return 'M';
		case WPAD_BUTTON_PLUS:	return 'P';
		case WPAD_BUTTON_1:	return '1';
		case WPAD_BUTTON_2:	return '2';
		case WPAD_BUTTON_HOME:	return 'H';
	}
	return 'x';
}

void Menu_Unlock() {
	u32 buttons;
	static long long t_start;
	long long t_now;
	unsigned ms_diff = 0;
	int i = 0;
	char buf[16];
	bool unlocked = false;
	memset(buf, 0, sizeof(buf));

	//init the previous settings
	if (unlock_init) {
		disable_format = CFG.disable_format;
		disable_remove = CFG.disable_remove;
		disable_install = CFG.disable_install;
		disable_options = CFG.disable_options;
		confirm_start = CFG.confirm_start;
		unlock_init = false;
	}
	
	Con_Clear();
	printf(gt("Configurable Loader %s"), CFG_VERSION);
	printf("\n\n");

	if (CFG.admin_mode_locked) {
		printf(gt("Enter Code: "));
		t_start = gettime();
		
		while (ms_diff < 30000 && !unlocked) {
			buttons = Wpad_GetButtons();
			if (buttons) {
				printf("*");
				buf[i] = get_unlock_buttons(buttons);
				i++;
				if (stricmp(buf, CFG.unlock_password) == 0) {
					unlocked = true;
				}
				if (i >= 10) break;
			}
			VIDEO_WaitVSync();
			t_now = gettime();
			ms_diff = diff_msec(t_start, t_now);
		}
	}
	if (unlocked) {
		//save existing settings
		disable_format = CFG.disable_format;
		disable_remove = CFG.disable_remove;
		disable_install = CFG.disable_install;
		disable_options = CFG.disable_options;
		confirm_start = CFG.confirm_start;

		//enable all "admin-type" screens
		CFG.disable_format = 0;
		CFG.disable_remove = 0;
		CFG.disable_install = 0;
		CFG.disable_options = 0;
		CFG.confirm_start = 1;
		CFG.admin_mode_locked = 0;
		printf("\n\n");
		printf(gt("SUCCESS!"));
		sleep(1);
		//show the hidden games
		__Menu_GetEntries();
	} else {
		//set the lock back on
		CFG.disable_format = disable_format;
		CFG.disable_remove = disable_remove;
		CFG.disable_install = disable_install;
		CFG.disable_options = disable_options;
		CFG.confirm_start = confirm_start;
		CFG.admin_mode_locked = 1;
		printf("\n\n");
		printf(gt("LOCKED!"));
		sleep(1);
		//reset the hidden games
		__Menu_GetEntries();
	}
}


int Menu_Views()
{
	struct discHdr *header = NULL;
	int redraw_cover = 0;
	struct Menu menu;

	if (gameCnt) {
		header = &gameList[gameSelected];
	}
	
	const int NUM_OPT = 8;
	char active[NUM_OPT];
	menu_init(&menu, NUM_OPT);

	for (;;) {

		menu.line_count = 0;
		menu_init_active(&menu, active, sizeof(active));

		//if admin lock is off or they're not in admin 
		// mode then they can't hide/unhide covers
		if (!CFG.admin_lock || CFG.admin_mode_locked) {
			if (CFG.disable_remove) active[3] = 0;
			if (CFG.disable_install) active[4] = 0;
		}
		if (CFG.disable_options) {
			active[1] = 0;
			active[2] = 0;
		}
		Con_Clear();
		FgColor(CFG.color_header);
		printf_x(gt("Main Menu"));
		printf(":\n\n");
		
		DefaultColor();
		MENU_MARK();
		printf("<%s>\n", gt("Start Game"));
		MENU_MARK();
		printf("<%s>\n", gt("Game Options"));
		MENU_MARK();
		printf("<%s>\n", gt("Global Options"));
		MENU_MARK();
		printf("<%s>\n", gt("Remove Game"));
		MENU_MARK();
		printf("<%s>\n", gt("Install Game"));
		MENU_MARK();
		printf("<%s>\n", gt("Sort Games"));
		MENU_MARK();
		printf("<%s>\n", gt("Filter Games"));
		MENU_MARK();
		printf("<%s>\n", gt("Boot Disc"));
   
		DefaultColor();

		printf("\n");
		printf_h(gt("Press %s to select"), (button_names[CFG.button_confirm.num]));
		printf("\n");
		DefaultColor();
		__console_flush(0);

		if (redraw_cover) {
			if (header) Gui_DrawCover(header->id);
			redraw_cover = 0;
		}
		
		u32 buttons = Wpad_WaitButtonsRpt();
		menu_move_active(&menu, buttons);
		
		int change = -2;
		if (buttons & WPAD_BUTTON_LEFT) change = -1;
		if (buttons & WPAD_BUTTON_RIGHT) change = +1;
		if (buttons & CFG.button_confirm.mask) change = 0;
//		#define CHANGE(V,M) {V+=change;if(V>M)V=M;if(V<0)V=0;}

		if (change > -2) {
			switch (menu.current) {
			case 0:
				CFG.confirm_start = 0;
				Menu_Boot(0);
				break;
			case 1:
				Menu_Game_Options();
				break;
			case 2:
				Menu_Global_Options();
				break;
			case 3:
				Menu_Remove();
				break;
			case 4:
				Menu_Install();
				break;
			case 5:
				Menu_Sort();
				break;
			case 6:
				Menu_Filter();
				break;
			case 7:
				Menu_Boot(1);
				break;
			}
		}

		// HOME button
		if (buttons & CFG.button_exit.mask) {
			Handle_Home(0);
		}
		if (buttons & CFG.button_cancel.mask) break;
	}
	
	return 0;
}

int Menu_Game_Options() {
	return Menu_Boot_Options(&gameList[gameSelected], 0);
}

int Menu_Boot_Options(struct discHdr *header, bool disc) {

	int ret_val = 0;
	if (CFG.disable_options) return 0;
	if (!gameCnt && !disc) {
		// if no games, go directly to global menu
		return 1;
	}

	struct Game_CFG_2 *game_cfg2 = NULL;
	struct Game_CFG *game_cfg = NULL;
	int opt_saved, opt_ios_reload; 
	f32 size = 0.0;
	int redraw_cover = 0;
	DOL_LIST dol_list;
	int i;
	int rows, cols, win_size;
	CON_GetMetrics(&cols, &rows);
	if ((win_size = rows-9) < 3) win_size = 3;
	Con_Clear();
	FgColor(CFG.color_header);
	printf_x(gt("Selected Game"));
	printf(":");
	__console_flush(0);
	if (disc) {
		printf(" (%.6s)\n", header->id);
	} else {
		WBFS_GameSize(header->id, &size);
		printf(" (%.6s) (%.2f%s)\n", header->id, size, gt("GB"));
	}
	DefaultColor();
	printf(" %s %s\n\n", CFG.cursor_space, __Menu_PrintTitle(get_title(header)));
	__console_flush(0);
	if (!disc) WBFS_GetDolList(header->id, &dol_list);

	game_cfg2 = CFG_get_game(header->id);
	if (!game_cfg2) {
		printf(gt("ERROR game opt"));
		printf("\n");
		sleep(5);
		return 0;
	}
	game_cfg = &game_cfg2->curr;

	struct Menu menu;
	const int NUM_OPT = 16;
	char active[NUM_OPT];
	menu_init(&menu, NUM_OPT);

	for (;;) {
		/*
		// fat on 249?
		if (wbfs_part_fs && !disc) {
			if (!is_ios_idx_mload(game_cfg->ios_idx))
			{
				game_cfg->ios_idx = CFG_IOS_222_MLOAD;
			}
		}
		*/

		menu_init_active(&menu, active, sizeof(active));
		opt_saved = game_cfg2->is_saved;
		// if not mload disable block ios reload opt
		opt_ios_reload = game_cfg->block_ios_reload;
		if (!is_ios_idx_mload(game_cfg->ios_idx)) {
			active[8] = 0;
			opt_ios_reload = 0;
		}
		// if not ocarina and not wiird, deactivate hooks
		if (!game_cfg->ocarina && !CFG.wiird) {
			active[11] = 0;
		}
		//if admin lock is off or they're not in admin 
		// mode then they can't hide/unhide games
		if (!CFG.admin_lock || CFG.admin_mode_locked) {
			active[14] = 0;
		}

		//These things shouldn't be changed if using a disc...maybe
		if (disc) {
			active[0] = 0;
			active[8] = 0;
			active[9] = 0;
			active[14] = 0;
		}
		
		Con_Clear();
		FgColor(CFG.color_header);
		printf_x(gt("Selected Game"));
		printf(":");
		if (disc) printf(" (%.6s)\n", header->id);
		else printf(" (%.6s) (%.2fGB)\n", header->id, size);
		DefaultColor();
		printf(" %s %s\n\n", CFG.cursor_space, __Menu_PrintTitle(get_title(header)));
		FgColor(CFG.color_header);
		printf_x(gt("Game Options:  %s"),
				CFG_is_changed(header->id) ? gt("[ CHANGED ]") :
				opt_saved ? gt("[ SAVED ]") : "");
		printf("\n");
		DefaultColor();
		char c1 = '<', c2 = '>';
		//if (opt_saved) { c1 = '['; c2 = ']'; }
		char str_alt_dol[32] = "Off";
		if (!disc) {
			for (i=0; i<dol_list.num; i++) {
				if (strcmp(game_cfg->dol_name, dol_list.name[i]) == 0) {
					game_cfg->alt_dol = 3 + i;
					break;
				}
			}
	
			switch (game_cfg->alt_dol) {
				case 0: STRCOPY(str_alt_dol, gt("Off")); break;
				case 1: STRCOPY(str_alt_dol, "SD"); break;
				case 2:
				default:
					if (*game_cfg->dol_name) {
						STRCOPY(str_alt_dol, game_cfg->dol_name);
					} else {
						STRCOPY(str_alt_dol, gt("Disc"));
					}
					break;
			}
		}
		const char *str_vpatch[3];
		str_vpatch[0] = gt("Off");
		str_vpatch[1] = gt("On");
		str_vpatch[2] = gt("All");

		// start menu draw

		menu.line_count = 0;
		menu_jump_active(&menu);

		#define PRINT_OPT_S(N,V) \
			printf("%s%c %s %c\n", con_align(N,18), c1, V, c2)

		#define PRINT_OPT_A(N,V) \
			printf("%s%c%s%c\n", con_align(N,18), c1, V, c2)

		#define PRINT_OPT_B(N,V) \
			PRINT_OPT_S(N,(V?gt("On"):gt("Off"))) 

		menu_window_begin(&menu, win_size, NUM_OPT);
		if (menu_window_mark(&menu))
			PRINT_OPT_S(gt("Favorite:"), is_favorite(header->id) ? gt("Yes") : gt("No"));
		if (menu_window_mark(&menu))
			PRINT_OPT_S(gt("Language:"), languages[game_cfg->language]);
		if (menu_window_mark(&menu))
			PRINT_OPT_S(gt("Video:"), videos[game_cfg->video]);
		if (menu_window_mark(&menu))
			PRINT_OPT_S(gt("Video Patch:"), str_vpatch[game_cfg->video_patch]);
		if (menu_window_mark(&menu))
			PRINT_OPT_B("VIDTV:", game_cfg->vidtv);
		if (menu_window_mark(&menu))
			PRINT_OPT_B(gt("Country Fix:"), game_cfg->country_patch);
		if (menu_window_mark(&menu))
			PRINT_OPT_B(gt("Anti 002 Fix:"), game_cfg->fix_002);
		if (menu_window_mark(&menu))
			PRINT_OPT_S("IOS:", ios_str(game_cfg->ios_idx));
		if (menu_window_mark(&menu))
			PRINT_OPT_B(gt("Block IOS Reload:"), opt_ios_reload);
		if (menu_window_mark(&menu))
		{
			char tmp[40] = "";
			snprintf(tmp, 40, "%s [%d]:", gt("Alt dol"), dol_list.num);
			PRINT_OPT_S(tmp, str_alt_dol);
		}
		if (menu_window_mark(&menu))
			PRINT_OPT_B(gt("Ocarina (cheats):"), game_cfg->ocarina);
		if (menu_window_mark(&menu))
			PRINT_OPT_S(gt("Hook Type:"), hook_name[game_cfg->hooktype]);
		if (menu_window_mark(&menu))
			PRINT_OPT_A(gt("Cheat Codes:"), gt("Manage"));
		if (menu_window_mark(&menu))
			printf("%s%s\n", con_align(gt("Cover Image:"), 18), 
				imageNotFound ? gt("< DOWNLOAD >") : gt("[ FOUND ]"));
		if (menu_window_mark(&menu))
			PRINT_OPT_S(gt("Hide Game:"), is_hide_game(header->id) ? gt("Yes") : gt("No"));
		if (menu_window_mark(&menu))
			PRINT_OPT_B(gt("Write Playlog:"), game_cfg->write_playlog);
		DefaultColor();
		menu_window_end(&menu, cols);

		printf_h(gt("Press %s to start game"), (button_names[CFG.button_confirm.num]));
		printf("\n");
		bool need_save = !opt_saved || CFG_is_changed(header->id);
		if (need_save)
			printf_h(gt("Press %s to save options"), (button_names[CFG.button_save.num]));
		else
			printf_h(gt("Press %s to discard options"), (button_names[CFG.button_save.num]));
		printf("\n");
		printf_h(gt("Press %s for global options"), (button_names[CFG.button_other.num]));
		DefaultColor();
		__console_flush(0);

		if (redraw_cover) {
			Gui_DrawCover(header->id);
			redraw_cover = 0;
		}
		
		u32 buttons = Wpad_WaitButtonsRpt();
		int change = 0;

		menu_move_active(&menu, buttons);

		if (buttons & WPAD_BUTTON_LEFT) change = -1;
		if (buttons & WPAD_BUTTON_RIGHT) change = +1;

		if (change) {
			switch (menu.current) {
			case 0:
				printf("\n\n");
				printf_x(gt("Saving Settings... "));
				__console_flush(0);
				if (set_favorite(header->id, change > 0)) {
					printf(gt("OK"));
				} else {
					printf(gt("ERROR"));
					sleep(1);
				}
				__console_flush(0);
				Gui_DrawCover(header->id);
				break;
			case 1:
				CHANGE(game_cfg->language, CFG_LANG_MAX);
				break;
			case 2:
				CHANGE(game_cfg->video, CFG_VIDEO_MAX);
				break;
			case 3:
				CHANGE(game_cfg->video_patch, 2);
				break;
			case 4:
				CHANGE(game_cfg->vidtv, 1);
				break;
			case 5:
				CHANGE(game_cfg->country_patch, 1);
				break;
			case 6:
				CHANGE(game_cfg->fix_002, 1);
				break;
			case 7:
				CHANGE(game_cfg->ios_idx, CFG_IOS_MAX);
				break;
			case 8:
				CHANGE(game_cfg->block_ios_reload, 1);
				break;
			case 9:
				CHANGE(game_cfg->alt_dol, 2 + dol_list.num);
				if (game_cfg->alt_dol < 3) {
					*game_cfg->dol_name = 0;
				} else {
					STRCOPY(game_cfg->dol_name, dol_list.name[game_cfg->alt_dol-3]);
				}
				break;
			case 10:
				CHANGE(game_cfg->ocarina, 1);
				break;
			case 11:
				CHANGE(game_cfg->hooktype, NUM_HOOK-1);
				break;
			case 12:
				Menu_Cheats(header);
				break;
			case 13:
				printf("\n\n");
				Download_Cover((char*)header->id, change > 0, true);
				Cache_Invalidate();
				Gui_DrawCover(header->id);
				Menu_PrintWait();
				break;
			case 14: // hide game
				printf("\n\n");
				printf_x(gt("Saving Settings... "));
				__console_flush(0);
				if (set_hide_game(header->id, change > 0)) {
					printf(gt("OK"));
				} else {
					printf(gt("ERROR"));
					sleep(1);
				}
				__console_flush(0);
				Gui_DrawCover(header->id);
				break;
			case 15:
				CHANGE(game_cfg->write_playlog, 1);
				break;
			}
		}
		if (buttons & CFG.button_confirm.mask) {
			CFG.confirm_start = 0;
			Menu_Boot(disc);
			break;
		}
		if (buttons & CFG.button_save.mask) {
			bool ret;
			printf("\n\n");
			if (need_save) {
				ret = CFG_save_game_opt(header->id);
				if (ret) {
					printf_x(gt("Options saved for this game."));
				} else printf(gt("Error saving options!")); 
			} else {
				ret = CFG_discard_game_opt(header->id);
				if (ret) printf_x(gt("Options discarded for this game."));
				else printf(gt("Error discarding options!")); 
			}
			sleep(1);
		}
		// HOME button
		if (buttons & CFG.button_exit.mask) {
			Handle_Home(0);
		}
		if (buttons & CFG.button_other.mask) { ret_val = 1; break; }
		if (buttons & CFG.button_cancel.mask) break;
	}
	CFG_release_game(game_cfg2);
	// refresh favorites list
	if (!disc) Switch_Favorites(enable_favorite);
	return ret_val;
}

void Save_Game_List()
{
	struct discHdr *buffer = NULL;
	u32 cnt, len;
	s32 ret;
	char name[200];
	FILE *f;
	int i;

	// Get Game List
	ret = WBFS_GetCount(&cnt);
	if (ret < 0) return;

	printf_x(gt("Saving gamelist.txt ... "));
	__console_flush(0);

	len = sizeof(struct discHdr) * cnt;
	buffer = (struct discHdr *)memalign(32, len);
	if (!buffer) goto error;
	memset(buffer, 0, len);
	ret = WBFS_GetHeaders(buffer, cnt, sizeof(struct discHdr));
	if (ret < 0) goto error;

	snprintf(D_S(name), "%s/%s", USBLOADER_PATH, "gamelist.txt");
	f = fopen(name, "wb");
	if (!f) goto error;
	fprintf(f, "# CFG USB Loader game list %s\n", CFG_VERSION);
	for (i=0; i<gameCnt; i++) {
		fprintf(f, "# %.6s %s\n", gameList[i].id, gameList[i].title);
		fprintf(f, "%.6s = %s\n", gameList[i].id, get_title(&gameList[i]));
	}
	fclose(f);
	SAFE_FREE(buffer);
	printf("OK");
	return;

	error:
	SAFE_FREE(buffer);
	printf(gt("ERROR"));
}

int Menu_Global_Options()
{
	int rows, cols, win_size = 11;
	CON_GetMetrics(&cols, &rows);
	if (strcmp(LAST_CFG_PATH, USBLOADER_PATH)) win_size += 2;
	if (CFG.ios_mload) win_size += 1;
	if ((win_size = rows-win_size) < 3) win_size = 3;

	if (CFG.disable_options) return 0;

	struct discHdr *header = NULL;
	int redraw_cover = 0;

	struct Menu menu;
	menu_init(&menu, 9);

	for (;;) {

		menu.line_count = 0;

		if (gameCnt) {
			header = &gameList[gameSelected];
		} else {
			header = NULL;
		}

		Con_Clear();
		FgColor(CFG.color_header);
		printf_x(gt("Global Options"));
		printf(":\n\n");
		DefaultColor();
		menu_window_begin(&menu, win_size, 9);
		if (menu_window_mark(&menu))
			printf("<%s>\n", gt("Main Menu"));
		if (menu_window_mark(&menu))
			printf("%s%2d/%-2d< %s > (%d)\n", con_align(gt("Profile:"),8),
				CFG.current_profile + 1, CFG.num_profiles,
				CFG.profile_names[CFG.current_profile],
				CFG.num_favorite_game);
		if (menu_window_mark(&menu))
			printf("%s%2d/%2d < %s >\n", con_align(gt("Theme:"),7),
				cur_theme + 1, num_theme, *CFG.theme ? CFG.theme : gt("none"));
		if (menu_window_mark(&menu))
			printf("%s< %s >\n", con_align(gt("Device:"),13),
				(wbfsDev == WBFS_DEVICE_USB) ? "USB" : "SDHC");
		if (menu_window_mark(&menu))
			printf("%s< %s >\n", con_align(gt("Partition:"),13), CFG.partition);
		if (menu_window_mark(&menu))
			printf("<%s>\n", gt("Download All Missing Covers"));
		if (menu_window_mark(&menu))
			printf("<%s>\n", gt("Update WiiTDB Game Database")); // download database - lustar
		if (menu_window_mark(&menu))
			printf("<%s>\n", gt("Update titles.txt"));
		if (menu_window_mark(&menu))
			printf("<%s>\n", gt("Check For Updates"));
		DefaultColor();
		menu_window_end(&menu, cols);
		
		printf_h(gt("Press %s for game options"), (button_names[CFG.button_other.num]));
		printf("\n");
		printf_h(gt("Press %s to save global settings"), (button_names[CFG.button_save.num]));
		printf("\n\n");
		Print_SYS_Info();
		DefaultColor();
		__console_flush(0);

		if (redraw_cover) {
			if (header) Gui_DrawCover(header->id);
			redraw_cover = 0;
		}
		
		u32 buttons = Wpad_WaitButtonsRpt();
		menu_move(&menu, buttons);

		int change = 0;
		if (buttons & WPAD_BUTTON_LEFT) change = -1;
		if (buttons & WPAD_BUTTON_RIGHT) change = +1;
		if (buttons & CFG.button_confirm.mask) change = +1;

		if (change) {
			switch (menu.current) {
			case 0:
				Menu_Views();
				return 0;
			case 1:
				CHANGE(CFG.current_profile, CFG.num_profiles-1);
				// refresh favorites list
				Switch_Favorites(enable_favorite);
				redraw_cover = 1;
				break;
			case 2:
				CFG_switch_theme(cur_theme + change);
				redraw_cover = 1;
				Cache_Invalidate();
				break;
			case 3:
				Menu_Device();
				return 0;
			case 4:
				Menu_Partition(true);
				return 0;
			case 5:
				Download_All_Covers(change > 0);
				Cache_Invalidate();
				if (header) Gui_DrawCover(header->id);
				Menu_PrintWait();
				break;
			case 6:
				Download_XML();
				break;
			case 7:
				Download_Titles();
				break;
			case 8:
				Online_Update();
				break;
			}
		}
		// HOME button
		if (buttons & CFG.button_exit.mask) {
			Handle_Home(0);
		}
		if (buttons & CFG.button_save.mask) {
			int ret;
			printf("\n");
			printf_x(gt("Saving Settings... "));
			printf("\n");
			__console_flush(0);
			FgColor(CFG.color_inactive);
			ret = CFG_Save_Global_Settings();
			DefaultColor();
			if (ret) {
				printf_(gt("OK"));
				printf("\n");
				Save_Game_List();
			} else {
				printf_(gt("ERROR"));
			}
			printf("\n");
			//sleep(2);
			Menu_PrintWait();
		}
		if (buttons & WPAD_BUTTON_PLUS) {
			printf("\n");
			mem_stat();
			Menu_PrintWait();
		}
		if (buttons & CFG.button_other.mask) return 1;
		if (buttons & CFG.button_cancel.mask) break;
	}
	return 0;
}

void Menu_Options()
{
	int ret = 1;
	while(ret)
	{
		if (gameCnt) {
			ret = Menu_Game_Options();
		}
		if (ret) {
			ret = Menu_Global_Options();
		}
	}
}

bool go_gui = false;
extern int action_alpha;

void DoAction(int action)
{
	if (action & CFG_BTN_REMAP) return;
	switch(action) {
		case CFG_BTN_NOTHING:
			break;
		case CFG_BTN_OPTIONS: 
			if (!CFG.disable_options) Menu_Options();
			break;
		case CFG_BTN_GUI:
			if (go_gui) {
				action_string[0] = 0;
				action_alpha=0;
			}
			if (CFG.gui) go_gui = !go_gui;
			break;
		case CFG_BTN_REBOOT:
			Con_Clear();
			Restart();
			break;
		case CFG_BTN_EXIT:
			Con_Clear();
			printf("\n");
			printf_("Exiting...");
			__console_flush(0);
			Sys_Exit();
			break;
		case CFG_BTN_SCREENSHOT:
			__console_flush(0);
			Make_ScreenShot();
			CFG.home = CFG_HOME_EXIT;
			break;
		case CFG_BTN_INSTALL:
			Menu_Install();
			break;
		case CFG_BTN_REMOVE:
			Menu_Remove();
			break;
		case CFG_BTN_MAIN_MENU: 
			Menu_Views();
			break;
		case CFG_BTN_GLOBAL_OPS:
			if (!CFG.disable_options) Menu_Global_Options();
			break;
		case CFG_BTN_PROFILE: 
			if (CFG.current_profile == CFG.num_profiles-1)
				CFG.current_profile = 0;
			else
				CFG.current_profile++;
			Switch_Favorites(enable_favorite);
			
			sprintf(action_string, gt("Profile: %s"), CFG.profile_names[CFG.current_profile]);
			
			break;
		case CFG_BTN_FAVORITES:
			{
				extern void reset_sort_default();
				reset_sort_default();
				Switch_Favorites(!enable_favorite);
			}
			break;
		case CFG_BTN_BOOT_GAME:
			Menu_Boot(0);
			break;
		case CFG_BTN_BOOT_DISC:
			Menu_Boot(1);
			break;
		case CFG_BTN_THEME:
			CFG_switch_theme(cur_theme + 1);
			if (gameCnt) Gui_DrawCover(gameList[gameSelected].id);//redraw_cover = 1;
			Cache_Invalidate();
			
			sprintf(action_string, gt("Theme: %s"), theme_list[cur_theme]);
			if (go_gui) action_alpha = 0xFF;
			
			break;
		case CFG_BTN_UNLOCK:
			if (CFG.admin_lock) Menu_Unlock();
			break;
		case CFG_BTN_HBC:
			Con_Clear();
			printf("\n");
			printf_("HBC...");
			__console_flush(0);
			Sys_HBC();
			break;
		case CFG_BTN_SORT:
			if (sort_desc) {
				sort_desc = 0;
				if (sort_index == sortCnt - 1)
					sort_index = 0;
				else
					sort_index = sort_index + 1;
				sortList(sortTypes[sort_index].sortAsc);
			} else {
				sort_desc = 1;
				sortList(sortTypes[sort_index].sortDsc);
			}
			if (gameCnt) Gui_DrawCover(gameList[gameSelected].id);//redraw_cover = 1;

			
			sprintf(action_string, gt("Sort: %s-%s"), sortTypes[sort_index].name, (sort_desc) ? "DESC":"ASC");
			
			break;
		case CFG_BTN_FILTER:
			Menu_Filter();
			break;
		default:
			// Priiloader magic words, and channels
			if ((action & 0xFF) < 'a') {
				// upper case final letter implies channel
				Con_Clear();
				Sys_Channel(action);
			} else {
				// lower case final letter implies magic word
				Con_Clear();
				*(vu32*)0x8132FFFB = action;
				Restart();
			}
			break;
	}
}

void __Menu_Controls(void)
{
	if (CFG.gui == CFG_GUI_START) {
		go_gui = true;
		goto gui_mode;
	}

	//u32 buttons = Wpad_WaitButtons();
	u32 buttons = Wpad_WaitButtonsRpt();

	/* UP/DOWN buttons */
	if (buttons & WPAD_BUTTON_UP)
		__Menu_MoveList(-1);

	if (buttons & WPAD_BUTTON_DOWN)
		__Menu_MoveList(1);

	/* LEFT/RIGHT buttons */
	if (buttons & WPAD_BUTTON_LEFT) {
		//__Menu_MoveList(-ENTRIES_PER_PAGE);
		if (CFG.cursor_jump) {
			__Menu_MoveList(-CFG.cursor_jump);
		} else {
			__Menu_MoveList((gameSelected-gameStart == 0) ? -ENTRIES_PER_PAGE : -(gameSelected-gameStart));
		}
	}

	if (buttons & WPAD_BUTTON_RIGHT) {
		//__Menu_MoveList(ENTRIES_PER_PAGE);
		if (CFG.cursor_jump) {
			__Menu_MoveList(CFG.cursor_jump);
		} else {
			__Menu_MoveList((gameSelected-gameStart == (ENTRIES_PER_PAGE - 1)) ? ENTRIES_PER_PAGE : ENTRIES_PER_PAGE - (gameSelected-gameStart) - 1);
		}
	}

	check_buttons:


	if (CFG.admin_lock) {
		if (buttons & CFG.button_other.mask) {

			static long long t_start;
			long long t_now;
			unsigned ms_diff = 0;
			bool display_unlock = false;

			Con_Clear();
			t_start = gettime();
			while (!display_unlock && (Wpad_Held(0) & CFG.button_other.mask)) {
				buttons = Wpad_GetButtons();
				VIDEO_WaitVSync();
				t_now = gettime();
				ms_diff = diff_msec(t_start, t_now);
				if (ms_diff > 5000)
					display_unlock = true;
			}
			if (display_unlock)
				Menu_Unlock();
			else
				buttons = buttonmap[MASTER][CFG.button_other.num];
		}
	}

	/* A button */
	//if (buttons & CFG.button_confirm.mask)
	//	Menu_Boot(0);

	int i;
	for (i = 4; i < MAX_BUTTONS; i++) {
			if (buttons & buttonmap[MASTER][i]) 
				DoAction(*(&CFG.button_M + (i - 4)));
	}
		
	//if (buttons & CFG.button_cancel.mask)
	//	DoAction(CFG.button_B);

	///* HOME button */
	//if (buttons & CFG.button_exit.mask) {
	//	DoAction(CFG.button_H);
	//	//Handle_Home(1);
	//}

	///* PLUS (+) button */
	//if (buttons & WPAD_BUTTON_PLUS)
	//	DoAction(CFG.button_P);
	////	Menu_Install();

	///* MINUS (-) button */
	//if (buttons & WPAD_BUTTON_MINUS)
	//	DoAction(CFG.button_M);
	////	Menu_Views();
	////	Menu_Remove();

	//if (buttons & WPAD_BUTTON_2)
	//	DoAction(CFG.button_2);

	//if (buttons & CFG.button_other.mask)
	//	DoAction(CFG.button_1);

	//if (buttons & WPAD_BUTTON_X)
	//	DoAction(CFG.button_X);

	//if (buttons & WPAD_BUTTON_Y)
	//	DoAction(CFG.button_Y);

	//if (buttons & WPAD_BUTTON_Z)
	//	DoAction(CFG.button_Z);

	//if (buttons & WPAD_BUTTON_C)
	//	DoAction(CFG.button_C);

	//if (buttons & WPAD_BUTTON_L)
	//	DoAction(CFG.button_L);

	//if (buttons & WPAD_BUTTON_R)
	//	DoAction(CFG.button_R);

	//// button 2 - switch favorites
	//if (buttons & CFG.button_save.mask) {
	//	extern void reset_sort_default();
	//	reset_sort_default();
	//	Switch_Favorites(!enable_favorite);
	//}


	//if (CFG.gui) {
	//	if (CFG.buttons == CFG_BTN_OPTIONS_1) {
	//		if (buttons & CFG.button_cancel.mask) go_gui = true;
	//	} else if (CFG.buttons == CFG_BTN_OPTIONS_B) {
	//		if (buttons & CFG.button_other.mask) go_gui = true;
	//	}
	//}
	//if (!CFG.disable_options) {
	//	if (CFG.buttons == CFG_BTN_OPTIONS_1) {
	//		if (buttons & CFG.button_other.mask) Menu_Options();
	//	} else if (CFG.buttons == CFG_BTN_OPTIONS_B) {
	//		if (buttons & CFG.button_cancel.mask) Menu_Options();
	//	} else { 
	//		/* ONE (1) button */
	//		if (buttons & CFG.button_other.mask) {
	//			//Menu_Device();
	//			Menu_Options();
	//		}
	//	}
	//}
	
	if (go_gui) {
		gui_mode:;
		int prev_sel = gameSelected;
		CFG.gui = 1; // disable auto start
		buttons = Gui_Mode();
		if (prev_sel != gameSelected) {
			// List scrolling
			__Menu_ScrollStartList();
		}
		// if only returning to con mode, clear button status
		/*if (CFG.buttons == CFG_BTN_OPTIONS_1) {
			if (buttons & CFG.button_cancel.mask) buttons = 0;
		} else if (CFG.buttons == CFG_BTN_OPTIONS_B) {
			if (buttons & CFG.button_other.mask) buttons = 0;
		}
		*/
		// if action started from gui, process it then return to gui
		//if (buttons) {
		//	go_gui = true;
		goto check_buttons;
		//}
	}
}

const char *get_dev_name(int dev)
{
	switch (dev) {
		case WBFS_DEVICE_USB: return gt("USB Mass Storage Device");
		case WBFS_DEVICE_SDHC: return gt("SD/SDHC Card");
	}
	return gt("Unknown");
}

void print_part(int i, PartList *plist)
{
	partitionEntry *entry = &plist->pentry[i];
	PartInfo *pinfo = &plist->pinfo[i];
	f32 size = entry->size * (plist->sector_size / GB_SIZE);
	if (plist->num == 1) {
		printf(gt("RAW"));
	} else {
		printf("%s", i<4 ? "P" : "L");
		printf("#%d", i+1);
	}
	printf(" %7.2f", size);
	//printf(" %-5s", part_type_name(entry->type));
	//printf(" %02x", entry->type);
	//printf(" %02x", entry->sector);
	if (pinfo->wbfs_i) {
		bool is_ext = part_is_extended(entry->type);
		printf(" ");
		if (is_ext) {
			printf(gt("EXTEND"));
			printf("/");
		}
		printf("WBFS%d", pinfo->wbfs_i);
		if (WBFS_Mounted() && wbfs_part_idx == pinfo->wbfs_i) {
			if (!is_ext) printf("      ");
			printf(gt("[USED]"));
		}
	} else if (pinfo->fat_i) {
		printf(" FAT%d ", pinfo->fat_i);
		if (wbfsDev == WBFS_DEVICE_USB && entry->sector == fat_usb_sec) {
			printf(" usb: ");
		} else if (wbfsDev == WBFS_DEVICE_SDHC && entry->sector == fat_sd_sec) {
			printf(" sd:  ");
		//} else if (wbfsDev == WBFS_DEVICE_USB && entry->sector == fat_wbfs_sec) {
		} else if (entry->sector == fat_wbfs_sec) {
			//printf(" wbfs:");
			printf(" %s", wbfs_fs_drive);
		} else {
			printf("      ");
		}
		//if (wbfsDev == WBFS_DEVICE_USB && entry->sector == wbfs_part_lba) {
		if (entry->sector == wbfs_part_lba) {
			printf(gt("[USED]"));
		}
	} else if (pinfo->ntfs_i) {
		printf(" NTFS%d ", pinfo->ntfs_i);
		if (entry->sector == fs_ntfs_sec) {
			//printf(" ntfs:");
			printf(" %s", wbfs_fs_drive);
		}
		if (entry->sector == wbfs_part_lba) {
			printf(" ");
			printf(gt("[USED]"));
		}
	} else {
		char *pt = part_type_name(entry->type);
		char *fs = get_fs_name(pinfo->fs_type);
		printf(" %s", pt);
		if (*fs == 0 && part_is_data(entry->type)) {
			if (strncasecmp(pt, "fat", 3) == 0
					|| strncasecmp(pt, "ntfs", 4) == 0)
			{
				fs = "-";
			}
		}
		if (strcmp(pt, fs) != 0 && *fs) {
			printf("/%s", fs);
		}
	}
}


void Menu_Format_Partition(partitionEntry *entry, bool delete_fs)
{
	int ret;

	if (CFG.disable_format) return;

	printf_x(gt("%s, please wait..."), delete_fs?gt("Deleting"):gt("Formatting"));
	__console_flush(0);

	// remount if overwriting mounted fat
	int re_usb = 0;
	int re_sd = 0;
	if (wbfsDev == WBFS_DEVICE_USB && entry->sector == fat_usb_sec) {
		re_usb = fat_usb_mount;
	}
	if (wbfsDev == WBFS_DEVICE_SDHC && entry->sector == fat_sd_sec) {
		re_sd = fat_sd_mount;
	}
	if (re_usb || re_sd) {
		Music_Pause();
		// unmount FAT
		if (re_usb) Fat_UnmountUSB();
		if (re_sd) Fat_UnmountSDHC();
	}
	// WBFS_Close will unmount fat/wbfs too
	WBFS_Close();
	all_gameCnt = gameCnt = 0;

	/* Format partition */
	bool ok;
	if (delete_fs) {
		char buf[512];
		memset(buf,0,sizeof(buf));
		ret = -2;
		ok = Device_WriteSectors(wbfsDev, entry->sector, 1, buf);
		if (ok) ret = 0;
	} else {
		ret = WBFS_Format(entry->sector, entry->size);
		ok = (ret == 0);
	}
	if (!ok) {
		printf("\n");
		printf_(gt("ERROR! (ret = %d)"), ret);
		printf("\n");
	} else {
		printf(" ");
		printf(gt("OK!"));
		printf("\n");
	}

	// try to remount FAT if was unmounted
	if (re_usb || re_sd) {
		if (re_usb) Fat_MountUSB();
		if (re_sd) Fat_MountSDHC();
		Music_UnPause();
	}

	printf("\n");
	Menu_PrintWait();
}

void Menu_Partition(bool must_select)
{
	PartList plist;
	partitionEntry *entry;
	PartInfo *pinfo;

	int i;
	s32 ret = 0;

rescan:

	/* Get partition entries */
	ret = Partition_GetList(wbfsDev, &plist);
	if (ret < 0) {
		printf_x(gt("ERROR: No partitions found! (ret = %d)"), ret);
		printf("\n");
		sleep(4);
		return;
	}

	struct Menu menu;
	char active[MAX_PARTITIONS_EX];
	menu_init(&menu, plist.num);
	menu_init_active(&menu, active, MAX_PARTITIONS_EX);

loop:
	menu_begin(&menu);
	/* Clear console */
	Con_Clear();

	FgColor(CFG.color_header);
	printf_x(gt("Select a partition"));
	printf(":\n\n");
	DefaultColor();
	printf_("[ %s ]\n\n", get_dev_name(wbfsDev));

	printf_("P# Size(GB) Type Mount Used\n");
	printf_("-----------------------------\n");
	//       P#1  400.00 FAT1  usb:  
	//       P#2  400.00 FAT2  wbfs: [USED]
	//       P#3  400.00 WBFS1       
	//       P#4  400.00 NTFS        

	int invalid_ext = -1;
	for (i = 0; i < plist.num; i++) {
		partitionEntry *entry = &plist.pentry[i];
		active[i] = part_valid_data(entry);
		MENU_MARK();
		print_part(i, &plist);
		printf("\n");
		if (part_is_extended(entry->type) && plist.pinfo[i].wbfs_i && i < 4) {
			invalid_ext = i;
		}
	}
	printf("\n");
	printf_h(gt("Press %s button to select."), (button_names[CFG.button_confirm.num]));
	printf("\n");
	printf_h(gt("Press %s button to go back."), (button_names[CFG.button_cancel.num]));
	printf("\n");
	if (!CFG.disable_format) {
		printf_h(gt("Press %s button to format WBFS."), (button_names[NUM_BUTTON_PLUS]));
		printf("\n");
		printf_h(gt("Press %s button to delete FS."), (button_names[NUM_BUTTON_MINUS]));
		printf("\n");
		if (invalid_ext >= 0) {
			printf_("\n");
			printf_(gt("NOTE: partition P#%d is type EXTEND but\n"
					"contains a WBFS filesystem. This is an\n"
					"invalid setup. Press %s to change the\n"
					"partition type from EXTEND to data."), invalid_ext+1, (button_names[CFG.button_other.num]));
			printf("\n");
			printf_h(gt("Press %s button to fix EXTEND/WBFS."), (button_names[CFG.button_other.num]));
			printf("\n");
		}
	}

	u32 buttons = Wpad_WaitButtonsCommon();

	menu_move(&menu, buttons);

	// B button
	if (buttons & CFG.button_cancel.mask) {
		if (must_select) {
			if (WBFS_Selected()) return;
			printf("\n");
			printf_(gt("No partition selected!"));
			printf("\n");
			sleep(2);
		} else {
			return;
		}
	}

	// A button
	if (buttons & CFG.button_confirm.mask) {
		i = menu.current;
		entry = &plist.pentry[i];
		pinfo = &plist.pinfo[i];
		int idx = pinfo->wbfs_i;
		int part_fs = PART_FS_WBFS;
		if (pinfo->fat_i) {
			printf(gt("Opening FAT partition..."));
			printf("\n");
			idx = pinfo->fat_i;
			part_fs = PART_FS_FAT;
		}
		if (pinfo->ntfs_i) {
			printf(gt("Opening NTFS partition..."));
			printf("\n");
			idx = pinfo->ntfs_i;
			part_fs = PART_FS_NTFS;
		}
		__console_flush(0);
		if (idx) {
			ret = WBFS_OpenPart(part_fs, idx, entry->sector,
					entry->size, CFG.partition);
			if (ret == 0 && WBFS_Selected()) {
				if (must_select) {
					// called from global options
					__Menu_GetEntries();
				}
				return;
			}
		}
	}

	// +/- button
	if ((buttons & WPAD_BUTTON_MINUS || buttons & WPAD_BUTTON_PLUS)
			&& !CFG.disable_format)
	{
		i = menu.current;
		entry = &plist.pentry[i];
		if (part_valid_data(entry)) {
			const char *act = gt("format");
			if (buttons & WPAD_BUTTON_MINUS) act = gt("delete");
			printf("\n");
			printf_x(gt("Are you sure you want to %s\n"
					"this partition?"), act);
			printf("\n\n");
			printf_("");
			print_part(i, &plist);
			printf("\n\n");
			if (Menu_Confirm(0)) {
				printf("\n");
				Menu_Format_Partition(entry, buttons & WPAD_BUTTON_MINUS);
				goto rescan;
			}
		}
	}
	// 1 button
	if (buttons & CFG.button_other.mask
		&& !CFG.disable_format
		&& invalid_ext >= 0)
	{
		printf("\n");
		printf_x(gt("Are you sure you want to FIX\n"
				"this partition?"));
		printf("\n\n");
		printf_("");
		print_part(invalid_ext, &plist);
		printf("\n\n");
		if (Menu_Confirm(0)) {
			printf("\n");
			printf_(gt("Fixing EXTEND partition..."));
			ret = Partition_FixEXT(wbfsDev, invalid_ext, plist.sector_size);
			printf("%s\n", ret ? gt("FAIL") : gt("OK"));
			printf_(gt("Press any button..."));
			Wpad_WaitButtonsCommon();
			goto rescan;
		}
	}

	goto loop;
}

void Menu_Device(void)
{
	u32 timeout = 30;
	s32 ret;
	static int first_time = 1;
	int save_wbfsDev = wbfsDev;

	printf_("");
	Fat_print_sd_mode();

	if (CFG.device == CFG_DEV_USB) {
		wbfsDev = WBFS_DEVICE_USB;
		printf("\n");
		printf_(gt("[ USB Mass Storage Device ]"));
		printf("\n\n");
		goto mount_dev;
	}
	if (CFG.device == CFG_DEV_SDHC) {
		wbfsDev = WBFS_DEVICE_SDHC;
		printf("\n");
		printf_(gt("[ SD/SDHC Card ]"));
		printf("\n\n");
		goto mount_dev;
	}
	restart:

	/* Ask user for device */
	for (;;) {
		const char *devname = gt("Unknown");

		/* Set device name */
		devname = get_dev_name(wbfsDev);

		//Enable the console
		Gui_Console_Enable();

		/* Clear console */
		Con_Clear();
			
		FgColor(CFG.color_header);
		printf_x(gt("Select WBFS device:"));
		printf("\n");
		DefaultColor();
		printf_("< %s >\n\n", devname);

		FgColor(CFG.color_help);
		printf_(gt("Press %s/%s to select device."), (button_names[NUM_BUTTON_LEFT]), (button_names[NUM_BUTTON_RIGHT]));
		printf("\n");
		printf_(gt("Press %s button to continue."), (button_names[CFG.button_confirm.num]));
		printf("\n");
		if (!first_time) {
			printf_(gt("Press %s button to cancel."), (button_names[CFG.button_cancel.num]));
			printf("\n");
		}
		printf("\n");

		Print_SYS_Info();

		u32 buttons = Wpad_WaitButtonsCommon();

		/* LEFT/RIGHT buttons */
		if (buttons & WPAD_BUTTON_LEFT) {
			if ((--wbfsDev) < WBFS_MIN_DEVICE)
				wbfsDev = WBFS_MAX_DEVICE;
		}
		if (buttons & WPAD_BUTTON_RIGHT) {
			if ((++wbfsDev) > WBFS_MAX_DEVICE)
				wbfsDev = WBFS_MIN_DEVICE;
		}

		/* A button */
		if (buttons & CFG.button_confirm.mask)
			break;

		// B button
		if (buttons & CFG.button_cancel.mask) {
		   	if (!first_time && WBFS_Selected()) {
				wbfsDev = save_wbfsDev;
				return;
			}
		}
	}

	mount_dev:
	CFG.device = CFG_DEV_ASK; // next time ask

	printf_x(gt("Mounting device, please wait..."));
	printf("\n");
	printf_(gt("(%d seconds timeout)"), timeout);
	printf("\n\n");
	fflush(stdout);

	/* Initialize WBFS */
	ret = WBFS_Init(wbfsDev, timeout);
	printf("\n");
	if (ret < 0) {
		//Enable the console
		Gui_Console_Enable();
		printf_(gt("ERROR! (ret = %d)"), ret);
		printf("\n");
		printf_(gt("Make sure USB port 0 is used!\n"
				"(The one nearest to the edge)"));
		printf("\n");

		/* Restart wait */
		Restart_Wait();
	}

	usleep(100000); // 100ms

	// Mount usb fat partition if not already
	// This is after device init, because of the timeout handling
	Fat_MountUSB();

	all_gameCnt = gameCnt = 0;
	/* Try to open device */
	//WBFS_Open();
	if (strcasecmp(CFG.partition, "ask") == 0) {
		goto jump_to_selection;
	}
	WBFS_OpenNamed(CFG.partition);

	while (!WBFS_Selected()) {

		//Enable the console
		Gui_Console_Enable();

		/* Clear console */
		Con_Clear();

		printf_x(gt("WARNING:"));
		printf("\n\n");

		printf_(gt("Partition: %s not found!"), CFG.partition);
		printf("\n");
		printf_(gt("Select a different partition"));
		printf("\n");
		if (!CFG.disable_format) {
			printf_(gt("or format a WBFS partition."));
			printf("\n");
		}
		printf("\n");

		printf_h(gt("Press %s button to select a partition."), (button_names[CFG.button_confirm.num]));
		printf("\n");
		printf_h(gt("Press %s button to change device."), (button_names[CFG.button_cancel.num]));
		printf("\n");
		printf_h(gt("Press %s button to exit."), (button_names[CFG.button_exit.num]));
		printf("\n\n");

		// clear button states
		WPAD_Flush(WPAD_CHAN_ALL);

		/* Wait for user answer */
		for (;;) {
			u32 buttons = Wpad_WaitButtonsCommon();
			if (buttons & CFG.button_confirm.mask) break;
			if (buttons & CFG.button_cancel.mask) goto restart;
		}

		// Select or Format partition
		jump_to_selection:
		Menu_Partition(false);
	}

	/* Get game list */
	__Menu_GetEntries();

	if (CFG.debug && first_time) {
		Menu_PrintWait();
	}
	first_time = 0;
}

u8 BCA_Data[64] ATTRIBUTE_ALIGN(32);

void Menu_DumpBCA(u8 *id)
{
	int ret;
	char fname[100];
	memset(BCA_Data, 0, 64);
	printf_("\n");
	printf_(gt("Reading BCA..."));
	printf("\n\n");
	ret = WDVD_Read_Disc_BCA(BCA_Data);
	hex_dump3(BCA_Data, 64);
	printf_("\n");
	if (ret) {
		printf_(gt("ERROR reading BCA!"));
		printf("\n\n");
		goto out;
	}
	// save
	snprintf(D_S(fname), "%s/%.6s.bca", USBLOADER_PATH, (char*)id);
	if (!Menu_Confirm(gt("save"))) return;
	printf("\n");
	printf_(gt("Writing: %s"), fname);
	printf("\n\n");
	FILE *f = fopen(fname, "wb");
	if (!f) {
		printf_(gt("ERROR writing BCA!"));
		printf("\n\n");
		goto out;
	}
	fwrite(BCA_Data, 64, 1, f);
	fclose(f);
	out:
	Menu_PrintWait();
}

void Menu_Install(void)
{
	static struct discHdr header ATTRIBUTE_ALIGN(32);

	s32 ret;

#ifdef FAKE_GAME_LIST
	fake_games++;
	__Menu_GetEntries();
	return;
#endif

	if (CFG.disable_install) return;

	/* Clear console */
	Con_Clear();

	FgColor(CFG.color_header);
	printf_x(gt("Install game"));
	printf("\n\n");
	DefaultColor();
	
	char *nocover = "ZZZZZZ";
	Gui_DrawCover((u8 *)nocover);

	/* Disable WBFS mode */
	Disc_SetWBFS(0, NULL);

	// Wait for disc
	//printf_x("Checking game disc...");
	u32 cover = 0;
	ret = WDVD_GetCoverStatus(&cover);
	if (ret < 0) {
		err1:
		printf("\n");
		printf_(gt("ERROR! (ret = %d)"), ret);
		printf("\n");
		goto out;
	}
	if (!(cover & 0x2)) {
		printf_(gt("Please insert a game disc..."));
		printf("\n\n");
		printf_h(gt("Press %s to return"), (button_names[CFG.button_cancel.num]));
		printf("\n\n");
		do {
			ret = WDVD_GetCoverStatus(&cover);
			if (ret < 0) goto err1;
			u32 buttons = Wpad_GetButtons();
			if (buttons & CFG.button_cancel.mask) {
				header = gameList[gameSelected];
				Gui_DrawCover(header.id);
				return;
			}
			VIDEO_WaitVSync();
		} while(!(cover & 0x2));
	}

	// Open disc
	printf_x(gt("Opening DVD disc..."));

	ret = Disc_Open();
	if (ret < 0) {
		printf("\n");
		printf_(gt("ERROR! (ret = %d)"), ret);
		printf("\n");
		goto out;
	} else {
		printf(" ");
		printf(gt("OK!"));
		printf("\n");
	}

	/* Check disc */
	ret = Disc_IsWii();
	if (ret < 0) {
		printf_x(gt("ERROR: Not a Wii disc!!"));
		printf("\n");
		goto out;
	}

	/* Read header */
	Disc_ReadHeader(&header);
	u64 comp_size = 0, real_size = 0;
	WBFS_DVD_Size(&comp_size, &real_size);

	Gui_DrawCover(header.id);
	
	printf("\n");
	__Menu_PrintInfo2(&header, comp_size, real_size);

	// force fat freespace update when installing
	extern int wbfs_fat_vfs_have;
	wbfs_fat_vfs_have = 0;
	// Disk free space
	f32 free, used, total;
	WBFS_DiskSpace(&used, &free);
	total = used + free;
	//printf_x("WBFS: %.1fGB free of %.1fGB\n\n", free, total);
	printf_x("%s: ", CFG.partition);
	printf(gt("%.1fGB free of %.1fGB"), free, total);
	printf("\n\n");

	bench_io();

	if (check_dual_layer(real_size, NULL)) print_dual_layer_note();

	/* Check if game is already installed */
	int way_out = 0;
	ret = WBFS_CheckGame(header.id);
	if (ret) {
		printf_x(gt("ERROR: Game already installed!!"));
		printf("\n\n");
		//goto out;
		way_out = 1;
	}
	// require +128kb for operating safety
	if ((f32)comp_size + (f32)128*1024 >= free * GB_SIZE) {
		printf_x(gt("ERROR: not enough free space!!"));
		printf("\n\n");
		//goto out;
		way_out = 1;
	}

	// get confirmation
	retry:
	if (!way_out) {
		printf_h(gt("Press %s button to continue."), (button_names[CFG.button_confirm.num]));
		printf("\n");
	}
	printf_h(gt("Press %s button to dump BCA."), (button_names[CFG.button_other.num]));
	printf("\n");
	printf_h(gt("Press %s button to go back."), (button_names[CFG.button_cancel.num]));
	printf("\n");
	DefaultColor();
	for (;;) {
		u32 buttons = Wpad_WaitButtonsCommon();
		if (!way_out)
			if (buttons & CFG.button_confirm.mask) break;
		if (buttons & CFG.button_cancel.mask) {
			WDVD_StopMotor();
			header = gameList[gameSelected];
			Gui_DrawCover(header.id);
			return;
		}
		if (buttons & CFG.button_other.mask) {
			Menu_DumpBCA(header.id);
			if (way_out) {
				header = gameList[gameSelected];
				Gui_DrawCover(header.id);
				return;
			}
			Con_Clear();
			printf_x(gt("Install game"));
			printf("\n\n");
			__Menu_PrintInfo2(&header, comp_size, real_size);
			printf_x(gt("WBFS: %.1fGB free of %.1fGB"), free, total);
			printf("\n\n");
			goto retry;
		}
	}

	printf_x(gt("Installing game, please wait..."));
	printf("\n\n");

	// Pause the music
	Music_Pause();

	/* Install game */
	ret = WBFS_AddGame();

	// UnPause the music
	Music_UnPause();

	if (ret < 0) {
		printf_x(gt("Installation ERROR! (ret = %d)"), ret);
		printf("\n");
		goto out;
	}

	/* Reload entries */
	__Menu_GetEntries();

	/* Turn on the Slot Light */
	wiilight(1);

out:
	// clear buttons status
	WPAD_Flush(WPAD_CHAN_ALL);
	printf("\n");
	printf_h(gt("Press button %s to eject DVD."), (button_names[CFG.button_confirm.num]));
	printf("\n");
	printf_h(gt("Press any button to continue..."));
	printf("\n\n");

	/* Wait for any button */
	u32 buttons = Wpad_WaitButtons();

	/* Turn off the Slot Light */
	wiilight(0);

	// button A
	if (buttons & CFG.button_confirm.mask) {
		printf_(gt("Ejecting DVD..."));
		printf("\n");
		WDVD_Eject();
		sleep(1);
	}
	header = gameList[gameSelected];
	Gui_DrawCover(header.id);
}

void Menu_Remove(void)
{
	struct discHdr *header = NULL;

	s32 ret;

#ifdef FAKE_GAME_LIST
	fake_games--;
	__Menu_GetEntries();
	return;
#endif

	if (CFG.disable_remove) return;

	/* No game list */
	if (!gameCnt)
		return;

	/* Selected game */
	header = &gameList[gameSelected];

	/* Clear console */
	Con_Clear();

	FgColor(CFG.color_header);
	printf_x(gt("Are you sure you want to remove this\n"
			"game?"));
	printf("\n\n");
	DefaultColor();

	/* Show game info */
	__Menu_PrintInfo(header);

	if (!Menu_Confirm(0)) return;
	printf("\n\n");

	printf_x(gt("Removing game, please wait..."));
	fflush(stdout);

	/* Remove game */
	ret = WBFS_RemoveGame(header->id);
	if (ret < 0) {
		printf("\n");
		printf_(gt("ERROR! (ret = %d)"), ret);
		printf("\n");
		goto out;
	} else {
		printf(" ");
		printf(gt("OK!"));
	}

	/* Reload entries */
	__Menu_GetEntries();

out:
	printf("\n");
	Menu_PrintWait();
}

extern s32 __WBFS_ReadDVD(void *fp, u32 lba, u32 len, void *iobuf);
extern int block_used(u8 *used,u32 i,u32 wblk_sz);

void bench_io()
{
	if (CFG.debug != 8) return;
	//int buf[64*1024/4];
	//int buf[32*1024/4];
	int size_buf = 128*1024;
	int size_total = 64*1024*1024;
	//int size_total = 256*1024;
	void *buf = memalign(32, size_buf);
	int i, ret;
	int ms = 0;
	int count = 0;
	int count_max = size_total / size_buf;
	int size;
	wiidisc_t *d = 0;
	u8 *used = 0;
	int n_wii_sec_per_disc = 143432*2;//support for double layers discs..
	int wii_sec_sz = 0x8000;
	int n_wii_sec_per_buf = size_buf / wii_sec_sz;

	//printf("\nread: %d\n", n_wii_sec_per_buf); __console_flush(0);
	printf("\n");
	printf(gt("Running benchmark, please wait"));
	printf("\n");
	__console_flush(0);

	used = calloc(n_wii_sec_per_disc,1);
	d = wd_open_disc(__WBFS_ReadDVD, NULL);
	if (!d) { printf(gt("unable to open wii disc")); goto out; }
	wd_build_disc_usage(d,ALL_PARTITIONS,used);
	wd_close_disc(d);
	d = 0;
	printf(gt("linear...")); printf("\n"); __console_flush(0);
	dbg_time1();
	for (i=0; i<n_wii_sec_per_disc / n_wii_sec_per_buf; i++) {
		if (block_used(used,i,n_wii_sec_per_buf)) {
			u32 offset = i * (size_buf>>2);
			ret = __WBFS_ReadDVD(NULL,offset, size_buf,buf);
			if (ret) {
				printf(" %d = %d\n", i, ret); __console_flush(0);
				Wpad_WaitButtonsCommon();
			}
			count++;
			if (count >= count_max) break;
		}
	}
	ms = dbg_time2("64mb read time (ms):");
	size = count * size_buf;
	printf("\n");
	printf(gt("linear read speed: %.2f mb/s"),
			(float)size / ms * 1000 / 1024 / 1024);
	printf("\n");

	count = 0;
	printf(gt("random...")); printf("\n"); __console_flush(0);
	dbg_time1();
	for (i = n_wii_sec_per_disc / n_wii_sec_per_buf - 1; i>0; i--) {
		if (block_used(used,i,n_wii_sec_per_buf)) {
			u32 offset = i * (size_buf>>2);
			ret = __WBFS_ReadDVD(NULL,offset, size_buf,buf);
			if (ret) {
				printf(" %d = %d\n", i, ret); __console_flush(0);
				Wpad_WaitButtonsCommon();
			}
			count++;
			if (count >= count_max) break;
		}
	}
	ms = dbg_time2("64mb read time (ms):");
	size = count * size_buf;
	printf("\n");
	printf(gt("random read speed: %.2f mb/s"),
			(float)size / ms * 1000 / 1024 / 1024);
	printf("\n");
out:
	printf("\n");
	Menu_PrintWait();
	//exit(0);
}

void Menu_Boot(bool disc)
{
	struct discHdr *header;
	bool gc = false;
	if (disc) {
		char *nocover = "ZZZZZZ";
		Gui_DrawCover((u8 *)nocover);
		header = (struct discHdr *)memalign(32, sizeof(struct discHdr));
	} else {
		header = &gameList[gameSelected];
	}
	s32 ret;
	struct Game_CFG_2 *game_cfg = NULL;
	u64 comp_size = 0, real_size = 0;

	/* No game list */
	if (!disc && !gameCnt)
		return;

	/* Clear console */
	if (!CFG.direct_launch || disc) {
		Con_Clear();
	}
	if (disc) {
		/* Disable WBFS mode */
		Disc_SetWBFS(0, NULL);
	
		FgColor(CFG.color_header);
		printf_x(gt("Start this game?"));
		printf("\n\n");
		DefaultColor();
		
		// Wait for disc
		u32 cover = 0;
		ret = WDVD_GetCoverStatus(&cover);
		if (ret < 0) {
			err1:
			printf("\n");
			printf_(gt("ERROR! (ret = %d)"), ret);
			printf("\n");
			goto out;
		}
		if (!(cover & 0x2)) {
			printf_(gt("Please insert a game disc..."));
			printf("\n\n");
			printf_h(gt("Press %s to return"), (button_names[CFG.button_cancel.num]));
			printf("\n\n");
			do {
				ret = WDVD_GetCoverStatus(&cover);
				if (ret < 0) goto err1;
				u32 buttons = Wpad_GetButtons();
				if (buttons & CFG.button_cancel.mask) goto close;
				VIDEO_WaitVSync();
			} while(!(cover & 0x2));
		}
	
		// Open disc
		printf_x(gt("Opening DVD disc..."));
	
		ret = Disc_Open();
		if (ret < 0) {
			printf("\n");
			printf_(gt("ERROR! (ret = %d)"), ret);
			printf("\n");
			goto out;
		} else {
			printf(" ");
			printf("OK!");
			printf("\n");
		}
	
		/* Check disc */
		ret = Disc_IsWii();
		if (ret < 0) {
			ret = Disc_IsGC();
			if (ret < 0) {
				printf_x(gt("ERROR: Not a Wii disc!!"));
				printf("\n");
				goto out;
			} else {
				gc = true;
			}
		}
		
		/* Read header */
		Disc_ReadHeader(header);
		Gui_DrawCover(header->id);
	}
	game_cfg = CFG_find_game(header->id);

	// Get game size
	if (!disc) WBFS_GameSize2(header->id, &comp_size, &real_size);
	bool dl_warn = check_dual_layer(real_size, game_cfg);
	bool can_skip = !CFG.confirm_start && !dl_warn && check_device(game_cfg, false);
	bool do_skip = (disc && !CFG.confirm_start) || (!disc && can_skip);


	SoundInfo snd;
	u8 banner_title[84];
	memset(banner_title, 0, 84);
	memset(&snd, 0, sizeof(snd));
	WBFS_Banner(header->id, &snd, banner_title, !do_skip, CFG_read_active_game_setting(header->id).write_playlog);

	if (do_skip) {
		printf("\n");
		/* Show game info */
		__Menu_PrintInfo(header);
			goto skip_confirm;
	}

	if (disc) {
		printf("\n");
	} else {
		Gui_Console_Enable();
		FgColor(CFG.color_header);
		printf_x(gt("Start this game?"));
		printf("\n\n");
		DefaultColor();
	}
	/* Show game info */
	if (disc) {
		printf_("%s\n", get_title(header));
		printf_("(%.6s)\n\n", header->id);
	} else {
		__Menu_PrintInfo(header);
	}
	__Menu_ShowGameInfo(true, header->id); /* load game info from XML */
	printf("\n");

	//Does DL warning apply to launching discs too? Not sure
	if (!disc) {
		check_device(game_cfg, true);
		if (dl_warn) print_dual_layer_note();
	}
	printf_h(gt("Press %s button to continue."), (button_names[CFG.button_confirm.num]));
	printf("\n");
	printf_h(gt("Press %s button to go back."), (button_names[CFG.button_cancel.num]));
	if (!gc) {
		printf("\n");
		printf_h(gt("Press %s button for options."), (button_names[CFG.button_other.num]));
	}
	printf("\n\n");
	__console_flush(0);

	// play banner sound
	if (snd.dsp_data) {
		SND_PauseVoice(0, 1); // pause mp3
		int fmt = (snd.channels == 2) ? VOICE_STEREO_16BIT : VOICE_MONO_16BIT;
		SND_SetVoice(1, fmt, snd.rate, 0,
			snd.dsp_data, snd.size,
			255,255, //volume,volume,
			NULL); //DataTransferCallback
	}

	/* Wait for user answer */
	u32 buttons;
	for (;;) {
		buttons = Wpad_WaitButtons();
		if (buttons & CFG.button_confirm.mask) break;
		if (buttons & CFG.button_cancel.mask) break;
		if (!gc && (buttons & CFG.button_other.mask)) break;
		if (buttons & CFG.button_exit.mask) break;
	}

	// stop banner sound, resume mp3
	if (snd.dsp_data) {
		SND_StopVoice(1);
		SAFE_FREE(snd.dsp_data);
		if (buttons & CFG.button_confirm.mask) {
			SND_ChangeVolumeVoice(0, 0, 0);
		}
		SND_PauseVoice(0, 0);
	}

	if (buttons & CFG.button_cancel.mask) goto close;
	if (buttons & CFG.button_exit.mask) {
		Handle_Home(0);
		return;
	}
	if (!gc && (buttons & CFG.button_other.mask)) {
		if (disc) Menu_Boot_Options(header, 1);
		else Menu_Options();
		return;
	}
	// A button: continue to boot

	skip_confirm:

	if (game_cfg) {
		CFG.game = game_cfg->curr;
		cfg_ios_set_idx(CFG.game.ios_idx);
		//dbg_printf("set ios: %d idx: %d\n", CFG.ios, CFG.game.ios_idx);
	}

	if (CFG.game.write_playlog && set_playrec(header->id, banner_title) < 0) {
		printf_(gt("Error storing playlog file.\nStart from the Wii Menu to fix."));
		printf("\n");
		printf_h(gt("Press %s button to exit."), (button_names[CFG.button_exit.num]));
		printf("\n");
		if (!Menu_Confirm(0)) return;
	}

	printf("\n");
	printf_x(gt("Booting Wii game, please wait..."));
	printf("\n\n");

	// If fat, open wbfs disc and verfy id as a consistency check
	if (!disc) {
		if (wbfs_part_fs) {
			wbfs_disc_t *d = WBFS_OpenDisc(header->id);
			if (!d) {
				printf_(gt("ERROR: opening %.6s"), header->id);
				printf("\n");
				goto out;
			}
			WBFS_CloseDisc(d);
			/*
			if (!is_ios_idx_mload(CFG.game.ios_idx))
			{
				printf(gt("Switching to IOS222 for FAT support."));
				printf("\n");
				CFG.game.ios_idx = CFG_IOS_222_MLOAD;
				cfg_ios_set_idx(CFG.game.ios_idx);
			}
			*/
		}
	}

	// load stuff before ios reloads & services close
	ocarina_load_code(header->id);
	load_wip_patches(header->id);
	load_bca_data(header->id);

	if (!disc) {
		ret = get_frag_list(header->id);
		if (ret) {
			printf_(gt("ERROR: get_frag_list: %d"), ret);
			printf("\n");
			goto out;
		}
	}

	// stop services (music, gui)
	Services_Close();

	setPlayStat(header->id); //I'd rather do this after the check, but now you unmount fat before that ;)
	
	//Incase booting of disc fails, just go back to loader. Bad practice?
	if (CFG.game.alt_dol != 1) {
		// unless we're loading alt.dol from sd
		// unmount everything
		Fat_UnmountAll();
	}

	if (!disc) {
		ret = ReloadIOS(1, 1);
		if (ret < 0) goto out;
		
		Block_IOS_Reload();
	
		// verify IOS version
		warn_ios_bugs();
	
		//dbg_time1();
	
		/* Set WBFS mode */
		ret = Disc_SetWBFS(wbfsDev, header->id);
	 	if (ret) {
	 		printf_(gt("ERROR: SetWBFS: %d"), ret);
			printf("\n");
	 		goto out;
	 	}
		
		/* Open disc */
		ret = Disc_Open();
	
		//dbg_time2("\ndisc open");
		//Wpad_WaitButtonsCommon();
		bench_io();
	
		if (ret < 0) {
			printf_(gt("ERROR: Could not open game! (ret = %d)"), ret);
			printf("\n");
			goto out;
		}
	}

	if (gc) {
		WII_Initialize();
		ret = WII_LaunchTitle(0x0000000100000100ULL);
	} else {
		switch(CFG.game.language)
				{
						// 0 = CFG_LANG_CONSOLE
						case 0: configbytes[0] = 0xCD; break; 
						case 1: configbytes[0] = 0x00; break; 
						case 2: configbytes[0] = 0x01; break; 
						case 3: configbytes[0] = 0x02; break; 
						case 4: configbytes[0] = 0x03; break; 
						case 5: configbytes[0] = 0x04; break; 
						case 6: configbytes[0] = 0x05; break; 
						case 7: configbytes[0] = 0x06; break; 
						case 8: configbytes[0] = 0x07; break; 
						case 9: configbytes[0] = 0x08; break; 
						case 10: configbytes[0] = 0x09; break;
				}

		/* Boot Wii disc */
		ret = Disc_WiiBoot(disc);
	}
	printf_(gt("Returned! (ret = %d)"), ret);
	printf("\n");

out:
	printf("\n");
	printf_(gt("Press any button to exit..."));
	printf("\n");

	/* Wait for button */
	Wpad_WaitButtonsCommon();
	exit(0);
close:
	if (disc) {
		WDVD_StopMotor();
		header = &gameList[gameSelected];
		Gui_DrawCover(header->id);
	}
	return;
}

void Direct_Launch()
{
	int i;

	if (!CFG.direct_launch) return;

	// enable console in case there is some input
	// required when loading game, depends on options, like:
	// confirm ocarina, alt_dol=disk (asks for dol)
	// but this can be moved to those points and removed from here
	// so that console is shown only if needed...
	if (CFG.intro) {
		Gui_Console_Enable();
	}

	// confirm_direct_start
	//CFG.confirm_start = 0;

	//sleep(1);
	Con_Clear();
	printf(gt("Configurable Loader %s"), CFG_VERSION);
	printf("\n\n");
	//sleep(1);
	for (i=0; i<gameCnt; i++) {
		if (strncmp(CFG.launch_discid, (char*)gameList[i].id, 6) == 0) {
			gameSelected = i;
			Menu_Boot(0);
			goto out;
		}
	}
	Gui_Console_Enable();
	printf(gt("Auto-start game: %.6s not found!"), CFG.launch_discid);
	printf("\n");
	printf(gt("Press any button..."));
	printf("\n");
	Wpad_WaitButtons();
	out:
	CFG.direct_launch = 0;
}

void Menu_Loop(void)
{
	// enable the console if starting with console mode
	if (CFG.gui != CFG_GUI_START) {
		if (!(CFG.direct_launch && !CFG.intro)) {
			Gui_Console_Enable();
		}
	}

	/* Device menu */
	Menu_Device();

	// Direct Launch?
	Direct_Launch();

	// Start Music
	Music_Start();

	// Clear console
	// (so that it doesn't show when switching back from gui)
	Con_Clear();
	__console_scroll = 0;

	// Init Favorites
	Switch_Favorites(CFG.start_favorites);

	// Start GUI
	if (CFG.gui == CFG_GUI_START) goto skip_list;

	/* Menu loop */
	for (;;) {
		/* Clear console */
		Con_Clear();

		/* Show gamelist */
		__Menu_ShowList();

		/* Show cover */
		__Menu_ShowCover();

		//memstat();

		skip_list:
		/* Controls */
		__Menu_Controls();
	}
}



// menu support routines

void menu_init(struct Menu *m, int num_opt)
{
	memset(m, 0, sizeof(*m));
	m->num_opt = num_opt;
}

void menu_begin(struct Menu *m)
{
	m->line_count = 0;
}

void menu_init_active(struct Menu *m, char *active, int active_size)
{
	m->active = active;
	m->active_size = active_size;
	if (!active) return;
	memset(active, 1, active_size);
}

char m_active(struct Menu *m, int i)
{
	if (m->active == NULL) return 1;
	if (i >= m->active_size) return 0;
	if (i < 0) return 0;
	return m->active[i];
}

void menu_jump_active(struct Menu *m)
{
	int i;
	if (m->current >= m->num_opt) m->current = m->num_opt - 1;
	if (m->current < 0) m->current = 0;
	if (m_active(m, m->current)) return;
	// move to first m->active
	for (i=0; i < m->num_opt; i++) {
		if (m_active(m, i)) {
			m->current = i;
			break;
		}
	}
	if (i == m->num_opt) m->current = 0;
}

void menu_move_cap(struct Menu *m)
{
	if (m->current >= m->num_opt) m->current = m->num_opt-1;
	if (m->current < 0) m->current = 0;
}

void menu_move_wrap(struct Menu *m)
{
	if (m->current >= m->num_opt) m->current = 0;
	if (m->current < 0) m->current = m->num_opt-1;
	if (m->current < 0) m->current = 0;
}

void menu_move_adir(struct Menu *m, int dir)
{
	int i, n;
	menu_move_cap(m);
	n = m->current;
	for (i=0; i<m->num_opt; i++) {
		m->current += dir;
		menu_move_wrap(m);
		if (m_active(m, m->current)) break;
	}
}

// only move on active lines
void menu_move_active(struct Menu *m, int buttons)
{
	if (buttons & WPAD_BUTTON_UP) {
		menu_move_adir(m, -1);
	}
	if (buttons & WPAD_BUTTON_DOWN) {
		menu_move_adir(m, 1);
	}
}

// simple, move on all lines
void menu_move(struct Menu *m, int buttons)
{
	int dir = 0;
	menu_move_cap(m);
	if (buttons & WPAD_BUTTON_UP) dir = -1;
	if (buttons & WPAD_BUTTON_DOWN) dir = 1;
	m->current += dir;
	menu_move_wrap(m);
}

void menu_print_mark(struct Menu *m)
{
	//if (m->active[m->line_count] && m->current == m->line_count) {
	if (m->current == m->line_count) {
		BgColor(CFG.color_selected_bg);
		FgColor(CFG.color_selected_fg);
		Con_ClearLine();
	} else if (m_active(m, m->line_count)) {
		DefaultColor();
	} else {
		DefaultColor();
		FgColor(CFG.color_inactive);
	}
	char *xx;
	//if (m->active[m->line_count] && m->current == m->line_count) {
	if (m->current == m->line_count) {
		xx = CFG.cursor;
	} else {
		xx = CFG.cursor_space;
	}
	printf(" %s ", xx);
}

int menu_mark(struct Menu *m)
{
	menu_print_mark(m);
	return m->line_count++;
}


// size is number of visible lines on screen
// num_items is number of items in the list
void menu_window_begin(struct Menu *m, int size, int num_items)
{
	if (num_items > size + 1)
		m->window_size = size;
	else
		m->window_size = size + 1;
	m->window_items = num_items;
	m->window_begin = m->line_count;
	// adjust window_pos so that selected line is inside the window
	// current_pos = position of selected line
	//   (relative offset from window_begin)
	int current_pos = m->current - m->window_begin;
	if (current_pos >= m->window_pos + m->window_size) {
		if (current_pos > num_items) {
			m->window_pos = num_items - m->window_size + 1;
		} else {
			m->window_pos = current_pos - m->window_size + 1;
		}
	} else if (current_pos < m->window_pos) {
		m->window_pos = current_pos;
	}
	if (m->window_pos < 0) m->window_pos = 0;
	// print page continuation marker
	// only if needed
	if (m->window_size < m->window_items) {
		if (m->window_pos > 0) {
			printf(" %s +\n", CFG.cursor_space);
		} else {
			printf("\n");
		}
	}
}

bool menu_window_mark(struct Menu *m)
{
	int pos = m->line_count - m->window_begin;
	if (pos < m->window_pos) {
		m->line_count++;
		return false;
	}

	if (pos >= m->window_pos + m->window_size) {
		m->line_count++;
		return false;
	}

	menu_mark(m);
	return true;
}

void menu_window_end(struct Menu *m, int cols)
{
	int pos = m->line_count - m->window_begin;
	char str[20] = "";
	int len;
	printf(" %s ", CFG.cursor_space);
	if (pos > m->window_pos + m->window_size) {
		printf("+");
	} else {
		printf(" ");
	}
	if (m->window_size < m->window_items)
		sprintf(str, "%s: %d/%d", gt("page"), 
			    (m->current - m->window_begin) / m->window_size + 1,
				(m->window_items-1) / m->window_size + 1);
	len = strlen(str);
	printf("%*.*s", cols-6-len, cols-6-len, str);
	printf("\n");
	// debug
	//printf("c:%d s:%d b:%d i:%d p:%d\n", m->current, m->window_size,
	//		m->window_begin, m->window_items, m->window_pos);
}


// indented printf
// will indent also each \n in string
void printf_a(const char *fmt, va_list argp)
{
	char strbuf[512];
	char *s, *n;
	int ret;
	ret = vsnprintf(strbuf, sizeof(strbuf), fmt, argp);
	if (ret >= sizeof(strbuf)) {
		// buffer too small, just print directly
		vprintf(fmt, argp);
	} else {
		// append space also to each \n
		s = strbuf;
		do {
			n = strchr(s, '\n');
			if (n) *n = 0; // terminate new line
			printf("%s", s);
			if (n) {
				printf("\n");
				s = n + 1;
				// only indent if there's more text to print
				// don't indent if \n is at the end of string
				if (*s == 0) break;
				printf("%s", CFG.menu_plus_s);
			}
		} while (n);
	}
}

void printf_(const char *fmt, ...)
{
	va_list argp;
	printf("%s", CFG.menu_plus_s);
	va_start(argp, fmt);
	printf_a(fmt, argp);
	va_end(argp);
}

void printf_x(const char *fmt, ...)
{
	va_list argp;
	printf("%s", CFG.menu_plus);
	va_start(argp, fmt);
	//vprintf(fmt, argp);
	printf_a(fmt, argp);
	va_end(argp);
}

void printf_h(const char *fmt, ...)
{
	va_list argp;
	DefaultColor();
	FgColor(CFG.color_help);
	printf("%s", CFG.menu_plus_s);
	va_start(argp, fmt);
	//vprintf(fmt, argp);
	printf_a(fmt, argp);
	va_end(argp);
	DefaultColor();
}

int Menu_PrintWait()
{
	printf_h(gt("Press any button to continue..."));
	printf("\n");
	// clear button states
	WPAD_Flush(WPAD_CHAN_ALL);
	return Wpad_WaitButtonsCommon();
}

bool Menu_Confirm(const char *msg)
{
	if (msg) {
		printf_h(gt("Press %s button to %s."), (button_names[CFG.button_confirm.num]), msg);
	} else {
		printf_h(gt("Press %s button to continue."), (button_names[CFG.button_confirm.num]));
	}
	printf("\n");
	printf_h(gt("Press %s button to go back."), (button_names[CFG.button_cancel.num]));
	printf("\n");
	DefaultColor();
	WPAD_Flush(WPAD_CHAN_ALL);
	for (;;) {
		u32 buttons = Wpad_WaitButtonsCommon();
		if (buttons & CFG.button_confirm.mask) return true;
		if (buttons & CFG.button_cancel.mask) return false;
	}
}

/*
Maybe:

Ocarina (cheats): < Off >

>> Compatibility  < Edit >
   Favorite:      < Yes >
   Hide Game:     < No >
   Cheat Codes:   < Manage >
   Cover Image:   < Download >
   < Start Game >
   < Remove Game >
   < Install Game >
   < Global Options >

*/

