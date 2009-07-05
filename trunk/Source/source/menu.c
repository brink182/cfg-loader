#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <unistd.h>
#include <sys/stat.h>

#include "disc.h"
#include "fat.h"
#include "gui.h"
#include "menu.h"
#include "partition.h"
#include "restart.h"
#include "sys.h"
#include "utils.h"
#include "video.h"
#include "wbfs.h"
#include "wpad.h"
#include "patchcode.h"
#include "cfg.h"
#include "http.h"
#include "dns.h"
#include "wdvd.h"
#include "music.h"

void Sys_Exit();

/* Constants */
/*
#define ENTRIES_PER_PAGE	12
#define MAX_CHARACTERS		30
*/

/* Gamelist buffer */
static struct discHdr *all_gameList = NULL;
static struct discHdr *fav_gameList = NULL;
struct discHdr *gameList = NULL;

/* Gamelist variables */
bool enable_favorite = false;
s32 all_gameCnt = 0;
s32 fav_gameCnt = 0;
s32 gameCnt = 0, gameSelected = 0, gameStart = 0;

bool imageNotFound = false;

/* WBFS device */
//static s32 wbfsDev = WBFS_MIN_DEVICE;
s32 wbfsDev = WBFS_MIN_DEVICE;

/*VIDEO OPTION - hungyip84 */
char videos[CFG_VIDEO_MAX+1][15] = 
{{"System Default"},
{"Game Default"},
{"Patch Game"},
{"Force PAL50"},
{"Force PAL60"},
{"Force NTSC"}
};

/*LANGUAGE PATCH - FISHEARS*/
char languages[11][22] =
{{"Console Default"},
{"Japanese"},
{"English"},
{"German"},
{"French"},
{"Spanish"},
{"Italian"},
{"Dutch"},
{"S. Chinese"},
{"T. Chinese"},
{"Korean"}};
/*LANGUAGE PATCH - FISHEARS*/

void wiilight(int enable);
void Switch_Favorites(bool enable);
void __Menu_ScrollStartList();

#if 0
// Debug Test mode with fake game list
#define WBFS_GetCount dbg_WBFS_GetCount
#define WBFS_GetHeaders dbg_WBFS_GetHeaders
s32 dbg_WBFS_GetCount(u32 *count);
s32 dbg_WBFS_GetHeaders(void *outbuf, u32 cnt, u32 len);
#endif

s32 __Menu_EntryCmp(const void *a, const void *b)
{
	struct discHdr *hdr1 = (struct discHdr *)a;
	struct discHdr *hdr2 = (struct discHdr *)b;
	char *title1 = get_title(hdr1);
	char *title2 = get_title(hdr2);

	/* Compare strings */
	return stricmp(title1, title2);
}

s32 __Menu_GetEntries(void)
{
	struct discHdr *buffer = NULL;

	u32 cnt, len;
	s32 ret;

	Cache_Invalidate();

	Switch_Favorites(false);
	if (fav_gameList) free(fav_gameList);
	fav_gameList = NULL;
	fav_gameCnt = 0;

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
	qsort(buffer, cnt, sizeof(struct discHdr), __Menu_EntryCmp);

	// hide and re-sort preffered
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
	//printf("fav %d %p %d\n", enable, fav_gameList, fav_gameCnt); sleep(2);
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
	// find same selected
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
	static char buffer[100];

	/* Clear buffer */
	memset(buffer, 0, sizeof(buffer));

	/* Check string length */
	if (strlen(name) >= MAX_CHARACTERS) {
		strncpy(buffer, name,  MAX_CHARACTERS - 4);
		strncat(buffer, "...", 3);

		return buffer;
	}

	return name;
}

void __Menu_PrintInfo(struct discHdr *header)
{
	f32 size = 0.0;

	/* Get game size */
	WBFS_GameSize(header->id, &size);

	/* Print game info */
	printf(" %s %s\n", CFG.cursor_space, get_title(header));
	printf(" %s (%.4s) (%.2fGB)\n\n", CFG.cursor_space, header->id, size);
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
	f32 free, used;

	/* Get free space */
	WBFS_DiskSpace(&used, &free);

	FgColor(CFG.color_header);
	if (enable_favorite) {
		printf("%sFavorite Games:\n", CFG.menu_plus);
	} else {
		if (!CFG.hide_header)
			printf("%sSelect the game you want to boot:\n", CFG.menu_plus);
	}
	printf("\n");
	DefaultColor();

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
			printf(" %s %s\n", (gameSelected == cnt) ? CFG.cursor : CFG.cursor_space,
				   __Menu_PrintTitle(get_title(header)));
		}
	} else
		printf("\t>> No games found!!\n");

	/* Print free/used space */
	FgColor(CFG.color_footer);
	BgColor(CONSOLE_BG_COLOR);
	if (!CFG.hide_hddinfo) {
		printf("\n%sUsed: %.1fGB Free: %.1fGB (%d)", CFG.menu_plus, used, free, gameCnt);
	}
	if (!CFG.hide_footer) {
		printf("\n");
		if (CFG.gui) {
			if (CFG.buttons == CFG_BTN_OPTIONS_1) {
				printf("%sPress 1 for options, B for GUI", CFG.menu_plus_s);
			} else if (CFG.buttons == CFG_BTN_OPTIONS_B) {
				printf("%sPress B for options, 1 for GUI", CFG.menu_plus_s);
			}
		} else {
			if (CFG.buttons == CFG_BTN_OPTIONS_1) {
				printf("%sPress button 1 for options", CFG.menu_plus_s);
			} else if (CFG.buttons == CFG_BTN_OPTIONS_B) {
				printf("%sPress button B for options", CFG.menu_plus_s);
			}
		}
	}
	DefaultColor();
	__console_flush(0);
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
		printf("\n    Exiting...");
		__console_flush(0);
		Sys_Exit();
	} else if (CFG.home == CFG_HOME_SCRSHOT) {
		__console_flush(0);
		Make_ScreenShot();
		if (disable_screenshot)	CFG.home = CFG_HOME_EXIT;
	} else {
		Con_Clear();
		Restart();
	}
}

void Print_SYS_Info()
{
	FgColor(CFG.color_inactive);
	printf("%s", CFG.menu_plus_s);
	Fat_print_sd_mode();
	printf("%sCFG base: %s\n", CFG.menu_plus_s, USBLOADER_PATH);
	printf("%sSD/USB Loader: 1.5+cfg33\n", CFG.menu_plus_s);
	printf("%sIOS%u (Rev %u)\n", CFG.menu_plus_s,
			IOS_GetVersion(), IOS_GetRevision());
	DefaultColor();
}

int Menu_Game_Options()
{
	int ret_val = 0;
	if (CFG.disable_options) return 0;
	if (!gameCnt) {
		// if no games, go directly to global menu
		return 1;
	}

	struct discHdr *header = &gameList[gameSelected];
	int current = 0;
	struct Game_CFG *game_cfg = NULL;
	int opt_saved, opt_ios_reload; 
	f32 size = 0.0;
	int redraw_cover = 0;

	WBFS_GameSize(header->id, &size);

	for (;;) {
		const int NUM_OPT = 11;
		char active[NUM_OPT];
		memset(active, 1, sizeof(active));
		game_cfg = CFG_get_game_opt(header->id);
		if (game_cfg) {
			opt_saved = 1;
			memset(active+1, 0, 9);
		} else {
			opt_saved = 0;
			game_cfg = &CFG.game;
		}
		// if not mload disable block ios reload opt
		opt_ios_reload = game_cfg->block_ios_reload;
		if (game_cfg->ios_idx != CFG_IOS_222_MLOAD &&
			game_cfg->ios_idx != CFG_IOS_223_MLOAD)
		{
			active[7] = 0;
			opt_ios_reload = 0;
		}
		if (!active[current]) {
			// move to first active
			for (current=0; current<NUM_OPT && !active[current]; current++);
		}
		Con_Clear();
		FgColor(CFG.color_header);
		printf("%sSelected Game:", CFG.menu_plus);
		printf(" (%.4s) (%.2fGB)\n", header->id, size);
		DefaultColor();
		printf(" %s %s\n\n", CFG.cursor_space, get_title(header));
		FgColor(CFG.color_header);
		printf("%sGame Options:  %s\n", CFG.menu_plus, opt_saved ? "[ SAVED ]" : "[ NOT SAVED ]");
		char c1 = '<', c2 = '>';
		int line_count = 0;
		if (opt_saved) { c1 = '['; c2 = ']'; }
		#define XX() ((active[line_count-1] && current == line_count-1) \
				? CFG.cursor : CFG.cursor_space)

		#define MARK_LINE() \
	   		if (active[line_count] && current == line_count) { \
				BgColor(CFG.color_selected_bg); FgColor(CFG.color_selected_fg); \
				Con_ClearLine(); \
			} else if (active[line_count]) { DefaultColor(); \
			} else { DefaultColor(); FgColor(CFG.color_inactive); } \
			line_count++;

		#define PRINT_OPT0(N) \
			printf(" %s "N"\n", XX())

		#define PRINT_OPT(NT,V) \
			printf(" %s "NT"\n", XX(), V)

		#define PRINT_OPTT(N,T,V) \
			printf(" %s "N"%c "T" %c\n", XX(), c1, V, c2)

		#define PRINT_OPTG(N,V) \
			PRINT_OPTT(N,"%s",V)	

		#define PRINT_OPTB(N,V) \
			PRINT_OPTG(N, (V?"On":"Off"))
		
		MARK_LINE();
		PRINT_OPT ("Favorite:         < %s >",
				is_favorite(header->id) ? "Yes" : "No");

		MARK_LINE();
		PRINT_OPTG("Video:       ", videos[game_cfg->video]);
		MARK_LINE();
		PRINT_OPTG("Language:    ", languages[game_cfg->language]);
		MARK_LINE();
		PRINT_OPTB("VIDTV:            ", game_cfg->vidtv);
		MARK_LINE();
		PRINT_OPTB("Country Fix:      ", game_cfg->country_patch);
		MARK_LINE();
		PRINT_OPTB("Anti 002 Fix:     ", game_cfg->fix_002);
		MARK_LINE();
		PRINT_OPTG("IOS:              ", ios_str(game_cfg->ios_idx));
		MARK_LINE();
		PRINT_OPTB("Block IOS Reload: ", opt_ios_reload);
		MARK_LINE();
		PRINT_OPTB("Alternative dol:  ", game_cfg->alt_dol);
		MARK_LINE();
		PRINT_OPTB("Ocarina:          ", game_cfg->ocarina);

		MARK_LINE();
		PRINT_OPT("Cover Image:     %s", 
				imageNotFound ? "< DOWNLOAD >" : "[ FOUND ]");

		DefaultColor();
		FgColor(CFG.color_help);
		printf("\n");
		printf("%sPress A to start game\n", CFG.menu_plus_s);
		if (opt_saved)
			printf("%sPress 2 to discard options\n", CFG.menu_plus_s);
		else
			printf("%sPress 2 to save options\n", CFG.menu_plus_s);
		printf("%sPress 1 for global options\n", CFG.menu_plus_s);
		DefaultColor();
		__console_flush(0);

		if (redraw_cover) {
			Gui_DrawCover(header->id);
			redraw_cover = 0;
		}
		
		u32 buttons = Wpad_WaitButtonsRpt();
		int change = 0, i;

		if (current < 0) current = 0;
		if (current >= NUM_OPT) current = NUM_OPT-1;
		if (buttons & WPAD_BUTTON_UP) {
			for (i=current-1; i >=0; i--)
				if (active[i]) { current = i; break; }
		}
		if (buttons & WPAD_BUTTON_DOWN) {
			for (i=current+1; i < NUM_OPT; i++)
				if (active[i]) { current = i; break; }
		}
		if (current < 0) current = 0;
		if (current >= NUM_OPT) current = NUM_OPT-1;

		if (buttons & WPAD_BUTTON_LEFT) change = -1;
		if (buttons & WPAD_BUTTON_RIGHT) change = +1;
		#define CHANGE(V,M) {V+=change;if(V<0)V=0;if(V>M)V=M;}

		if (change && (current < 1 || current > 9 || !opt_saved) ) {
			switch (current) {
			case 0:
				printf("\n%sSaving Settings... ", CFG.menu_plus);
				__console_flush(0);
				if (set_favorite(header->id, change > 0)) {
					printf("OK");
				} else {
					printf("ERROR");
					sleep(1);
				}
				__console_flush(0);
				Gui_DrawCover(header->id);
				break;
			case 1:
				CHANGE(CFG.game.video, CFG_VIDEO_MAX);
				break;
			case 2:
				CHANGE(CFG.game.language, CFG_LANG_MAX);
				break;
			case 3:
				CHANGE(CFG.game.vidtv, 1);
				break;
			case 4:
				CHANGE(CFG.game.country_patch, 1);
				break;
			case 5:
				CHANGE(CFG.game.fix_002, 1);
				break;
			case 6:
				CHANGE(CFG.game.ios_idx, CFG_IOS_MAX);
				cfg_ios("ios", ios_str(CFG.game.ios_idx));
				break;
			case 7:
				CHANGE(CFG.game.block_ios_reload, 1);
				break;
			case 8:
				CHANGE(CFG.game.alt_dol, 1);
				break;
			case 9:
				CHANGE(CFG.game.ocarina, 1);
				break;
			case 10:
				printf("\n");
				Download_Cover(header);
				Cache_Invalidate();
				Gui_DrawCover(header->id);
				sleep(1);
				break;
			}
		}
		if (buttons & WPAD_BUTTON_A) {
			CFG.confirm_start = 0;
			Menu_Boot();
			break;
		}
		if (buttons & WPAD_BUTTON_2) {
			bool ret;
			printf("\n");
			if (!opt_saved) {
				ret = CFG_save_game_opt(header->id);
				if (ret) printf("%sOptions saved for this game.", CFG.menu_plus);
				else printf("Error saving options!"); 
			} else {
				ret = CFG_forget_game_opt(header->id);
				if (ret) printf("%sOptions discarded for this game.", CFG.menu_plus);
				else printf("Error discarding options!"); 
			}
			sleep(1);
		}
		// HOME button
		if (buttons & WPAD_BUTTON_HOME) {
			Handle_Home(0);
		}
		if (buttons & WPAD_BUTTON_1) { ret_val = 1; break; }
		if (buttons & WPAD_BUTTON_B) break;
	}
	// refresh favorites list
	Switch_Favorites(enable_favorite);
	return ret_val;
}

int Menu_Global_Options()
{
	if (CFG.disable_options) return 0;

	struct discHdr *header = NULL;
	int current = 0;
	int opt_saved = 0;
	int redraw_cover = 0;

	if (gameCnt) {
		header = &gameList[gameSelected];
	}

	for (;;) {
		const int NUM_OPT = 3;
		char active[NUM_OPT];
		memset(active, 1, sizeof(active));
		if (!active[current]) {
			// move to first active
			for (current=0; current<NUM_OPT && !active[current]; current++);
		}
		Con_Clear();
		FgColor(CFG.color_header);
		printf("%sGlobal Options:\n\n", CFG.menu_plus);
		char c1 = '<', c2 = '>';
		int line_count = 0;
		if (opt_saved) { c1 = '['; c2 = ']'; }

		MARK_LINE();
		PRINT_OPT0("<Download All Missing Covers>");
		MARK_LINE();
		PRINT_OPT("Device:      < %s >",
				(wbfsDev == WBFS_DEVICE_USB) ? "USB" : "SDHC");
		MARK_LINE();
		printf(" %s Theme: %2d/%2d < %s >\n", XX(),
				cur_theme + 1, num_theme, *CFG.theme ? CFG.theme : "none");
		DefaultColor();

		FgColor(CFG.color_help);
		printf("\n");
		printf("%sPress 1 for game options\n", CFG.menu_plus_s);
		printf("\n");
		Print_SYS_Info();
		DefaultColor();
		__console_flush(0);

		if (redraw_cover) {
			if (header) Gui_DrawCover(header->id);
			redraw_cover = 0;
		}
		
		u32 buttons = Wpad_WaitButtonsRpt();
		int change = 0, i;

		if (current < 0) current = 0;
		if (current >= NUM_OPT) current = NUM_OPT-1;
		if (buttons & WPAD_BUTTON_UP) {
			for (i=current-1; i >=0; i--)
				if (active[i]) { current = i; break; }
		}
		if (buttons & WPAD_BUTTON_DOWN) {
			for (i=current+1; i < NUM_OPT; i++)
				if (active[i]) { current = i; break; }
		}
		if (current < 0) current = 0;
		if (current >= NUM_OPT) current = NUM_OPT-1;

		if (buttons & WPAD_BUTTON_LEFT) change = -1;
		if (buttons & WPAD_BUTTON_RIGHT) change = +1;

		if (change) {
			switch (current) {
			case 0:
				Download_All(change>0 ? true : false);
				Cache_Invalidate();
				if (header) Gui_DrawCover(header->id);
				break;
			case 1:
				Menu_Device();
				return 0;
			case 2:
				CFG_switch_theme(cur_theme + change);
				redraw_cover = 1;
				Cache_Invalidate();
				break;
			}
		}
		// HOME button
		if (buttons & WPAD_BUTTON_HOME) {
			Handle_Home(0);
		}
		if (buttons & WPAD_BUTTON_1) return 1;
		if (buttons & WPAD_BUTTON_B) break;
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

void __Menu_Controls(void)
{
	bool go_gui = false;
	if (CFG.gui == CFG_GUI_START) goto gui_mode;

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

	/* HOME button */
	if (buttons & WPAD_BUTTON_HOME) {
		Handle_Home(1);
	}

	/* PLUS (+) button */
	if (buttons & WPAD_BUTTON_PLUS)
		Menu_Install();

	/* MINUS (-) button */
	if (buttons & WPAD_BUTTON_MINUS)
		Menu_Remove();

	/* A button */
	if (buttons & WPAD_BUTTON_A)
		Menu_Boot();

	// button 2 - switch favorites
	if (buttons & WPAD_BUTTON_2) {
		Switch_Favorites(!enable_favorite);
	}

	if (CFG.gui) {
		if (CFG.buttons == CFG_BTN_OPTIONS_1) {
			if (buttons & WPAD_BUTTON_B) go_gui = true;
		} else if (CFG.buttons == CFG_BTN_OPTIONS_B) {
			if (buttons & WPAD_BUTTON_1) go_gui = true;
		}
	}
	if (!CFG.disable_options) {
		if (CFG.buttons == CFG_BTN_OPTIONS_1) {
			if (buttons & WPAD_BUTTON_1) Menu_Options();
		} else if (CFG.buttons == CFG_BTN_OPTIONS_B) {
			if (buttons & WPAD_BUTTON_B) Menu_Options();
		} else { 
			/* ONE (1) button */
			if (buttons & WPAD_BUTTON_1) {
				//Menu_Device();
				Menu_Options();
			}
		}
	}
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
		if (CFG.buttons == CFG_BTN_OPTIONS_1) {
			if (buttons & WPAD_BUTTON_B) buttons = 0;
		} else if (CFG.buttons == CFG_BTN_OPTIONS_B) {
			if (buttons & WPAD_BUTTON_1) buttons = 0;
		}
		// if action started from gui, process it then return to gui
		if (buttons) {
			go_gui = true;
			goto check_buttons;
		}
	}
}

void Menu_Format(void)
{
	partitionEntry partitions[MAX_PARTITIONS];

	u32 cnt, sector_size;
	s32 ret, selected = 0;

	/* Clear console */
	Con_Clear();

	/* Get partition entries */
	ret = Partition_GetEntries(wbfsDev, partitions, &sector_size);
	if (ret < 0) {
		printf("[+] ERROR: No partitions found! (ret = %d)\n", ret);

		/* Restart */
		Restart_Wait();
	}

loop:
	/* Clear console */
	Con_Clear();

	printf("[+] Selected the partition you want to \n");
	printf("    format:\n\n");

	/* Print partition list */
	for (cnt = 0; cnt < MAX_PARTITIONS; cnt++) {
		partitionEntry *entry = &partitions[cnt];

		/* Calculate size in gigabytes */
		f32 size = entry->size * (sector_size / GB_SIZE);

		/* Selected entry */
		(selected == cnt) ? printf(">> ") : printf("   ");
		fflush(stdout);

		/* Valid partition */
		if (size)
			printf("Partition #%d: (size = %.2fGB)\n",       cnt + 1, size);
		else 
			printf("Partition #%d: (cannot be formatted)\n", cnt + 1);
	}

	partitionEntry *entry = &partitions[selected];
	u32           buttons = Wpad_WaitButtons();

	/* UP/DOWN buttons */
	if (buttons & WPAD_BUTTON_UP) {
		if ((--selected) <= -1)
			selected = MAX_PARTITIONS - 1;
	}

	if (buttons & WPAD_BUTTON_DOWN) {
		if ((++selected) >= MAX_PARTITIONS)
			selected = 0;
	}

	/* B button */
	if (buttons & WPAD_BUTTON_B)
		return;

	/* Valid partition */
	if (entry->size) {
		/* A button */
		if (buttons & WPAD_BUTTON_A)
			goto format;
	}

	goto loop;

format:
	/* Clear console */
	Con_Clear();

	printf("[+] Are you sure you want to format\n");
	printf("    this partition?\n\n");

	printf("    Partition #%d\n",                  selected + 1);
	printf("    (size = %.2fGB - type: %02X)\n\n", entry->size * (sector_size / GB_SIZE), entry->type);

	printf("    Press A button to continue.\n");
	printf("    Press B button to go back.\n\n\n");

	// clear button states
	WPAD_Flush(WPAD_CHAN_ALL);

	/* Wait for user answer */
	for (;;) {
		u32 buttons = Wpad_WaitButtons();

		/* A button */
		if (buttons & WPAD_BUTTON_A)
			break;

		/* B button */
		if (buttons & WPAD_BUTTON_B)
			goto loop;
	}

	printf("[+] Formatting, please wait...");
	fflush(stdout);

	/* Format partition */
	ret = WBFS_Format(entry->sector, entry->size);
	if (ret < 0) {
		printf("\n    ERROR! (ret = %d)\n", ret);
		goto out;
	} else
		printf(" OK!\n");

out:
	printf("\n");
	printf("    Press any button to continue...\n");

	// clear button states
	WPAD_Flush(WPAD_CHAN_ALL);

	/* Wait for any button */
	Wpad_WaitButtons();
}

void Menu_Device(void)
{
	u32 timeout = 30;
	s32 ret;
	static int first_time = 1;

	printf("    ");
	Fat_print_sd_mode();

	if (CFG.device == CFG_DEV_USB) {
		wbfsDev = WBFS_DEVICE_USB;
		printf("\n    [ USB Mass Storage Device ]\n\n");
		goto mount_dev;
	}
	if (CFG.device == CFG_DEV_SDHC) {
		wbfsDev = WBFS_DEVICE_SDHC;
		printf("\n    [ SD/SDHC Card ]\n\n");
		goto mount_dev;
	}

	/* Ask user for device */
	for (;;) {
		char *devname = "Unknown!";

		/* Set device name */
		switch (wbfsDev) {
		case WBFS_DEVICE_USB:
			devname = "USB Mass Storage Device";
			break;

		case WBFS_DEVICE_SDHC:
			devname = "SD/SDHC Card";
			break;
		}

		/* Clear console */
		Con_Clear();

		FgColor(CFG.color_header);
		printf("[+] Select WBFS device:\n");
		DefaultColor();
		printf("    < %s >\n\n", devname);

		FgColor(CFG.color_help);
		printf("    Press LEFT/RIGHT to select device.\n");
		printf("    Press A button to continue.\n");
		if (!first_time) {
			printf("    Press B button to cancel.\n");
		}
		printf("\n");

		Print_SYS_Info();

		u32 buttons = Wpad_WaitButtons();

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
		if (buttons & WPAD_BUTTON_A)
			break;

		// B button
		if ((buttons & WPAD_BUTTON_B) && !first_time)
			return;
	}

	mount_dev:
	CFG.device = CFG_DEV_ASK; // next time ask

	printf("[+] Mounting device, please wait...\n");
	printf("    (%d seconds timeout)\n\n", timeout);
	fflush(stdout);

	/* Initialize WBFS */
	ret = WBFS_Init(wbfsDev, timeout);
	printf("\n");
	if (ret < 0) {
		printf("    ERROR! (ret = %d)\n", ret);

		/* Restart wait */
		Restart_Wait();
	}

	usleep(500000); // half sec

	/* Try to open device */
	while (WBFS_Open() < 0) {
		/* Clear console */
		Con_Clear();

		printf("[+] WARNING:\n\n");

		printf("    No WBFS partition found!\n");
		printf("    You need to format a partition.\n\n");

		if (CFG.disable_format) {
			Restart_Wait();
			return;
		}

		printf("    Press A button to format a partition.\n");
		printf("    Press B button to restart.\n\n");

		// clear button states
		WPAD_Flush(WPAD_CHAN_ALL);

		/* Wait for user answer */
		for (;;) {
			u32 buttons = Wpad_WaitButtons();

			/* A button */
			if (buttons & WPAD_BUTTON_A)
				break;

			/* B button */
			if (buttons & WPAD_BUTTON_B)
				Restart();
		}

		/* Format device */
		Menu_Format();
	}

	/* Get game list */
	__Menu_GetEntries();

	first_time = 0;
}

void Menu_Install(void)
{
	static struct discHdr header ATTRIBUTE_ALIGN(32);

	s32 ret;

	if (CFG.disable_install) return;

	/* Clear console */
	Con_Clear();

	FgColor(CFG.color_header);
	printf("[+] Are you sure you want to install a\n");
	printf("    new Wii game?\n\n");

	FgColor(CFG.color_help);
	printf("    Press A button to continue.\n");
	printf("    Press B button to go back.\n\n\n");
	DefaultColor();

	/* Wait for user answer */
	for (;;) {
		u32 buttons = Wpad_WaitButtons();

		/* A button */
		if (buttons & WPAD_BUTTON_A)
			break;

		/* B button */
		if (buttons & WPAD_BUTTON_B)
			return;
	}

	/* Disable WBFS mode */
	Disc_SetWBFS(0, NULL);

	printf("[+] Insert the game DVD disc...");
	fflush(stdout);

	/* Wait for disc */
	ret = Disc_Wait();
	if (ret < 0) {
		printf("\n    ERROR! (ret = %d)\n", ret);
		goto out;
	} else
		printf(" OK!\n");

	printf("[+] Opening DVD disc...");
	fflush(stdout);

	/* Open disc */
	ret = Disc_Open();
	if (ret < 0) {
		printf("\n    ERROR! (ret = %d)\n", ret);
		goto out;
	} else
		printf(" OK!\n\n");

	/* Check disc */
	ret = Disc_IsWii();
	if (ret < 0) {
		printf("[+] ERROR: Not a Wii disc!!\n");
		goto out;
	}

	/* Read header */
	Disc_ReadHeader(&header);

	/* Check if game is already installed */
	ret = WBFS_CheckGame(header.id);
	if (ret) {
		printf("[+] ERROR: Game already installed!!\n");
		goto out;
	}

	printf("[+] Installing game, please wait...\n\n");

	printf("    %s\n", get_title(&header));
	printf("    (%c%c%c%c)\n\n", header.id[0], header.id[1], header.id[2], header.id[3]);

	/* Install game */
	ret = WBFS_AddGame();
	if (ret < 0) {
		printf("[+] Installation ERROR! (ret = %d)\n", ret);
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
	printf("    Press button A to eject DVD.\n");
	printf("    Press any button to continue...\n");

	/* Wait for any button */
	u32 buttons = Wpad_WaitButtons();

	/* Turn off the Slot Light */
	wiilight(0);

	// button A
	if (buttons & WPAD_BUTTON_A) {
		printf("    Ejecting DVD...\n");
		WDVD_Eject();
		sleep(1);
	}
}

void Menu_Remove(void)
{
	struct discHdr *header = NULL;

	s32 ret;

	if (CFG.disable_remove) return;

	/* No game list */
	if (!gameCnt)
		return;

	/* Selected game */
	header = &gameList[gameSelected];

	/* Clear console */
	Con_Clear();

	FgColor(CFG.color_header);
	printf("[+] Are you sure you want to remove this\n");
	printf("    game?\n\n");
	DefaultColor();

	/* Show game info */
	__Menu_PrintInfo(header);

	FgColor(CFG.color_help);
	printf("    Press A button to continue.\n");
	printf("    Press B button to go back.\n\n\n");
	DefaultColor();

	/* Wait for user answer */
	for (;;) {
		u32 buttons = Wpad_WaitButtons();

		/* A button */
		if (buttons & WPAD_BUTTON_A)
			break;

		/* B button */
		if (buttons & WPAD_BUTTON_B)
			return;
	}

	printf("[+] Removing game, please wait...");
	fflush(stdout);

	/* Remove game */
	ret = WBFS_RemoveGame(header->id);
	if (ret < 0) {
		printf("\n    ERROR! (ret = %d)\n", ret);
		goto out;
	} else
		printf(" OK!\n");

	/* Reload entries */
	__Menu_GetEntries();

out:
	printf("\n");
	printf("    Press any button to continue...\n");

	/* Wait for any button */
	Wpad_WaitButtons();
}

void Menu_Boot(void)
{
	struct discHdr *header = NULL;

	s32 ret;
	struct Game_CFG *game_cfg = NULL;

	/* No game list */
	if (!gameCnt)
		return;

	/* Selected game */
	header = &gameList[gameSelected];

	game_cfg = CFG_get_game_opt(header->id);

	/* Clear console */
	Con_Clear();

	if (!CFG.confirm_start) {
		printf("\n");
		/* Show game info */
		__Menu_PrintInfo(header);
		goto skip_confirm;
	}

	/*
	printf("[+] Are you sure you want to boot this\n");
	printf("    game?\n\n");
	*/
	FgColor(CFG.color_header);
	printf("%sStart this game?\n\n", CFG.menu_plus);
	DefaultColor();

	/* Show game info */
	__Menu_PrintInfo(header);

	FgColor(CFG.color_help);
	printf("%sPress A button to continue.\n", CFG.menu_plus_s);
	printf("%sPress B button to go back.\n", CFG.menu_plus_s);
	printf("%sPress 1 button for options\n\n", CFG.menu_plus_s);
	DefaultColor();

	/* Wait for user answer */
	for (;;) {
		u32 buttons = Wpad_WaitButtons();

		/* A button */
		if (buttons & WPAD_BUTTON_A)
			break;

		/* B button */
		if (buttons & WPAD_BUTTON_B)
			return;

		// 1 button
		if (buttons & WPAD_BUTTON_1) {
			Menu_Options();
			return;
		}
	}

	skip_confirm:

	if (game_cfg) {
		CFG.game = *game_cfg;
		cfg_ios_set_idx(CFG.game.ios_idx);
	}

	printf("\n");
	printf("%sBooting Wii game, please wait...\n\n", CFG.menu_plus);

	// stop music
	Music_Stop();

	ret = ReloadIOS(1, 1);
	if (ret < 0) goto out;
	Block_IOS_Reload();

	/* Set WBFS mode */
	Disc_SetWBFS(wbfsDev, header->id);

	/* Open disc */
	ret = Disc_Open();
	if (ret < 0) {
		printf("    ERROR: Could not open game! (ret = %d)\n", ret);
		goto out;
	}

	switch(CFG.game.language)
                {
                        case 0: //CFG_LANG_CONSOLE
                                configbytes[0] = 0xCD;
                        break;

                        case 1:
                                configbytes[0] = 0x00;
                        break;

                        case 2:
                                configbytes[0] = 0x01;
                        break;

                        case 3:
                                configbytes[0] = 0x02;
                        break;

                        case 4:
                                configbytes[0] = 0x03;
                        break;

                        case 5:
                                configbytes[0] = 0x04;
                        break;

                        case 6:
                                configbytes[0] = 0x05;
                        break;

                        case 7:
                                configbytes[0] = 0x06;
                        break;

                        case 8:
                                configbytes[0] = 0x07;
                        break;

                        case 9:
                                configbytes[0] = 0x08;
                        break;

                        case 10:
                                configbytes[0] = 0x09;
                        break;
                }
	
	/* Boot Wii disc */
	Disc_WiiBoot();

	printf("    Returned! (ret = %d)\n", ret);

out:
	printf("\n");
	printf("    Press any button to continue...\n");

	/* Wait for button */
	Wpad_WaitButtons();
}


void Menu_Loop(void)
{
	/* Device menu */
	Menu_Device();

	// Start Music
	Music_Start();

	if (CFG.gui == CFG_GUI_START) goto skip_list;

	/* Menu loop */
	for (;;) {
		/* Clear console */
		Con_Clear();

		/* Show gamelist */
		__Menu_ShowList();

		/* Show cover */
		__Menu_ShowCover();

		skip_list:
		/* Controls */
		__Menu_Controls();
	}
}

// Thanks Dteyn for this nice feature =)
// Toggle wiilight (thanks Bool for wiilight source)
void wiilight(int enable)
{
	static vu32 *_wiilight_reg = (u32*)0xCD0000C0;
    u32 val = (*_wiilight_reg&~0x20);        
    if(enable) val |= 0x20;             
    *_wiilight_reg=val;            
}

