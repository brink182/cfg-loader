
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

/* Gamelist variables */
bool enable_favorite = false;
s32 all_gameCnt = 0;
s32 fav_gameCnt = 0;
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
{{"System Default"},
{"Game Default"},
{"Force PAL50"},
{"Force PAL60"},
{"Force NTSC"}
//{"Patch Game"},
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

void Switch_Favorites(bool enable);
void __Menu_ScrollStartList();
void bench_io();

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

s32 __Menu_EntryCmp(const void *a, const void *b)
{
	struct discHdr *hdr1 = (struct discHdr *)a;
	struct discHdr *hdr2 = (struct discHdr *)b;
	char *title1 = get_title(hdr1);
	char *title2 = get_title(hdr2);
	title1 = skip_sort_ignore(title1);
	title2 = skip_sort_ignore(title2);

	/* Compare strings */
	//return stricmp(title1, title2);
	return mbs_coll(title1, title2);
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
	int len = mbs_len(name);

	/* Clear buffer */
	memset(buffer, 0, sizeof(buffer));

	/* Check string length */
	if (len >= MAX_CHARACTERS) {
		//strncpy(buffer, name,  MAX_CHARACTERS - 4);
		STRCOPY(buffer, name);
		mbs_trunc(buffer, MAX_CHARACTERS - 4);
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
	if (IOS_GetVersion() != 249) {
		CFG.patch_dvd_check = 1;
		return;
	}
	// ios == 249
	if (wbfs_part_fs) {
		printf(
			"ERROR: CIOS222 or CIOS223 required for\n"
			"starting games from a FAT partition!\n\n");
		warn = true;
	}
	if (IOS_GetRevision() < 14) {
		printf(
			"NOTE: CIOS249 before rev14:\n"
			"Possible error #001 is not handled!\n\n");
		warn = true;
	}
	if (IOS_GetRevision() == 13) {
		printf(
			"NOTE: You are using CIOS249 rev13:\n"
			"To quit the game you must reset or\n"
			"power-off the Wii before you start\n"
			"another game or it will hang here!\n\n");
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
			"NOTE: CIOS249 before rev10:\n"
			"Loading games from SDHC not supported!\n\n");
		warn = true;
	}
	if (IOS_GetRevision() < 9
			&& wbfsDev == WBFS_DEVICE_USB) {
		printf(
			"NOTE: CIOS249 before rev9:\n"
			"Loading games from USB not supported!\n\n");
		warn = true;
	}
	if (warn) sleep(1);
}

void print_dual_layer_note()
{
	printf(
		"NOTE: You are using CIOS249 rev14:\n"
		"It has known problems with dual layer\n"
		"games. It is highly recommended that\n"
		"you use a different IOS or revision\n"
		"for installation/playback of this game.\n\n"
		);
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
		if (ii != CFG_IOS_222_MLOAD && ii != CFG_IOS_223_MLOAD) {
			if (print) printf(
				"ERROR: multiple wbfs partitions\n"
				"supported only with IOS222/223-mload\n\n");
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
		if ((IOS_GetVersion() == 249) && (IOS_GetRevision() == 14)) return true;
		return false;
	}
	if (game_cfg->curr.ios_idx != CFG_IOS_249) return false; 
	if (IOS_GetVersion() == 249) {
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
	printf_("(%.6s) (%.2fGB) %s\n\n", header->id, size, dl_str);
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
		printf_x("Favorite Games:\n");
	} else {
		if (!CFG.hide_header)
			printf_x("Select the game you want to boot:\n");
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
				printf("%*s", (MAX_CHARACTERS - strlen(title)),
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
		if (CFG.hide_hddinfo) {
			int num_page = 1 + (gameCnt - 1) / ENTRIES_PER_PAGE;
			int cur_page = 1 + gameSelected / ENTRIES_PER_PAGE;
			printf("%*s %d/%d", MAX_CHARACTERS - 8, " ", cur_page, num_page);
		}
	} else {
		printf(" %s No games found!!\n", CFG.cursor);
	}

	/* Print free/used space */
	FgColor(CFG.color_footer);
	BgColor(CONSOLE_BG_COLOR);
	if (!CFG.hide_hddinfo) {
		// Get free space
		f32 free, used;
		printf("\n");
		WBFS_DiskSpace(&used, &free);
		printf_x("Used: %.1fGB Free: %.1fGB [%d]", used, free, gameCnt);
		int num_page = 1 + (gameCnt - 1) / ENTRIES_PER_PAGE;
		int cur_page = 1 + gameSelected / ENTRIES_PER_PAGE;
		printf(" %d/%d", cur_page, num_page);
	}
	if (!CFG.hide_footer) {
		printf("\n");
		// (B) GUI (1) Options (2) Favorites
		// B: GUI 1: Options 2: Favorites
		char c_gui = 'B', c_opt = '1';
		if (CFG.buttons == CFG_BTN_OPTIONS_B) {
			c_gui = '1'; c_opt = 'B';
		}
		printf_("");
		if (CFG.gui) {
			printf("%c: GUI ", c_gui);
		}
		printf("%c: Options 2: Favorites", c_opt);
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
	} else if (CFG.home == CFG_HOME_HBC) {
		Con_Clear();
		printf("\n    HBC...");
		__console_flush(0);
		Sys_HBC();
	} else { // CFG_HOME_REBOOT
		Con_Clear();
		Restart();
	}
}

void Print_SYS_Info()
{
	extern int mload_ehc_fat;
	FgColor(CFG.color_inactive);
	printf_("");
	Fat_print_sd_mode();
	printf_("CFG base: %s\n", USBLOADER_PATH);
	if (strcmp(LAST_CFG_PATH, USBLOADER_PATH)) {
		// if last cfg differs, print it out
		printf_("Additional config:\n");
		printf_("  %s/config.txt\n", LAST_CFG_PATH);
	}
	printf_("Loader Version: %s\n", CFG_VERSION);
	printf_("IOS%u (Rev %u) %s\n",
			IOS_GetVersion(), IOS_GetRevision(),
			mload_ehc_fat ? "[FAT]" : "");
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
	printf("Configurable Loader %s\n\n", CFG_VERSION);

	if (CFG.admin_mode_locked) {
		printf("Enter Code: ");
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
		printf("\n\nSUCCESS!");
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
		printf("\n\nLOCKED!");
		sleep(1);
		//reset the hidden games
		__Menu_GetEntries();
	}
}

int Menu_Game_Options(){
	int ret_val = 0;
	if (CFG.disable_options) return 0;
	if (!gameCnt) {
		// if no games, go directly to global menu
		return 1;
	}

	struct discHdr *header = &gameList[gameSelected];
	struct Game_CFG_2 *game_cfg2 = NULL;
	struct Game_CFG *game_cfg = NULL;
	int opt_saved, opt_ios_reload; 
	f32 size = 0.0;
	int redraw_cover = 0;
	DOL_LIST dol_list;
	int i;

	Con_Clear();
	FgColor(CFG.color_header);
	printf_x("Selected Game:");
	__console_flush(0);
	WBFS_GameSize(header->id, &size);
	printf(" (%.6s) (%.2fGB)\n", header->id, size);
	DefaultColor();
	printf(" %s %s\n\n", CFG.cursor_space, __Menu_PrintTitle(get_title(header)));
	__console_flush(0);
	WBFS_GetDolList(header->id, &dol_list);

	game_cfg2 = CFG_get_game(header->id);
	if (!game_cfg2) {
		printf("ERROR game opt\n");
		sleep(5);
		return 0;
	}
	game_cfg = &game_cfg2->curr;

	struct Menu menu;
	const int NUM_OPT = 14;
	char active[NUM_OPT];
	menu_init(&menu, NUM_OPT);

	for (;;) {
		if (wbfs_part_fs) {
			if (game_cfg->ios_idx != CFG_IOS_222_MLOAD
					&& game_cfg->ios_idx != CFG_IOS_223_MLOAD)
			{
				game_cfg->ios_idx = CFG_IOS_222_MLOAD;
			}
		}

		menu_init_active(&menu, active, sizeof(active));
		opt_saved = game_cfg2->is_saved;
		// if not mload disable block ios reload opt
		opt_ios_reload = game_cfg->block_ios_reload;
		if (game_cfg->ios_idx != CFG_IOS_222_MLOAD &&
			game_cfg->ios_idx != CFG_IOS_223_MLOAD)
		{
			active[8] = 0;
			opt_ios_reload = 0;
		}
		//if admin lock is off or they're not in admin 
		// mode then they can't hide/unhide covers
		if (!CFG.admin_lock || CFG.admin_mode_locked) {
			active[13] = 0;
		}
		
		Con_Clear();
		FgColor(CFG.color_header);
		printf_x("Selected Game:");
		printf(" (%.6s) (%.2fGB)\n", header->id, size);
		DefaultColor();
		printf(" %s %s\n\n", CFG.cursor_space, __Menu_PrintTitle(get_title(header)));
		FgColor(CFG.color_header);
		printf_x("Game Options:  %s\n",
				CFG_is_changed(header->id) ? "[ CHANGED ]" :
				opt_saved ? "[ SAVED ]" : "");
		DefaultColor();
		char c1 = '<', c2 = '>';
		//if (opt_saved) { c1 = '['; c2 = ']'; }

		char str_alt_dol[32] = "Off";
		for (i=0; i<dol_list.num; i++) {
			if (strcmp(game_cfg->dol_name, dol_list.name[i]) == 0) {
				game_cfg->alt_dol = 3 + i;
				break;
			}
		}

		switch (game_cfg->alt_dol) {
			case 0: STRCOPY(str_alt_dol, "Off"); break;
			case 1: STRCOPY(str_alt_dol, "SD"); break;
			case 2:
			default:
				if (*game_cfg->dol_name) {
					STRCOPY(str_alt_dol, game_cfg->dol_name);
				} else {
					STRCOPY(str_alt_dol, "Disc");
				}
				break;
		}
		char *str_vpatch[] = { "Off", "On", "All" };

		// start menu draw

		menu.line_count = 0;
		menu_jump_active(&menu);

		#define PRINT_OPT_S(N,V) \
			printf("%s%c %s %c\n", N, c1, V, c2)

		#define PRINT_OPT_B(N,V) \
			PRINT_OPT_S(N,(V?"On":"Off")) 
		
		MENU_MARK();
		PRINT_OPT_S("Favorite:         ", is_favorite(header->id) ? "Yes" : "No");

		MENU_MARK();
		PRINT_OPT_S("Language:    ", languages[game_cfg->language]);
		MENU_MARK();
		PRINT_OPT_S("Video:       ", videos[game_cfg->video]);
		MENU_MARK();
		PRINT_OPT_S("Video Patch:      ", str_vpatch[game_cfg->video_patch]);
		MENU_MARK();
		PRINT_OPT_B("VIDTV:            ", game_cfg->vidtv);
		MENU_MARK();
		PRINT_OPT_B("Country Fix:      ", game_cfg->country_patch);
		MENU_MARK();
		PRINT_OPT_B("Anti 002 Fix:     ", game_cfg->fix_002);
		MENU_MARK();
		PRINT_OPT_S("IOS:              ", ios_str(game_cfg->ios_idx));
		MENU_MARK();
		PRINT_OPT_B("Block IOS Reload: ", opt_ios_reload);
		MENU_MARK();
		printf     ("Alt dol [%d found]:< %s >\n", dol_list.num, str_alt_dol);
		MENU_MARK();
		PRINT_OPT_B("Ocarina (cheats): ", game_cfg->ocarina);

		MENU_MARK();
		printf("Cheat Codes:     < Manage >\n");

		MENU_MARK();
		printf("Cover Image:     %s\n", 
				imageNotFound ? "< DOWNLOAD >" : "[ FOUND ]");
		MENU_MARK();
		printf("Hide Game:        < %s >\n",
				is_hide_game(header->id) ? "Yes" : "No");

		DefaultColor();
		printf("\n");
		printf_h("Press A to start game\n");
		bool need_save = !opt_saved || CFG_is_changed(header->id);
		if (need_save)
			printf_h("Press 2 to save options\n");
		else
			printf_h("Press 2 to discard options\n");
		printf_h("Press 1 for global options");
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
		#define CHANGE(V,M) {V+=change;if(V>M)V=M;if(V<0)V=0;}

		if (change) {
			switch (menu.current) {
			case 0:
				printf("\n\n");
				printf_x("Saving Settings... ");
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
				Menu_Cheats();
				break;
			case 12:
				printf("\n\n");
				Download_Cover((char*)header->id, change > 0, true);
				Cache_Invalidate();
				Gui_DrawCover(header->id);
				sleep(2);
				break;
			case 13:
				printf("\n\n");
				printf_x("Saving Settings... ");
				__console_flush(0);
				if (set_hide_game(header->id, change > 0)) {
					printf("OK");
				} else {
					printf("ERROR");
					sleep(1);
				}
				__console_flush(0);
				Gui_DrawCover(header->id);
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
			printf("\n\n");
			if (need_save) {
				ret = CFG_save_game_opt(header->id);
				if (ret) printf_x("Options saved for this game.");
				else printf("Error saving options!"); 
			} else {
				ret = CFG_discard_game_opt(header->id);
				if (ret) printf_x("Options discarded for this game.");
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
	CFG_release_game(game_cfg2);
	// refresh favorites list
	Switch_Favorites(enable_favorite);
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

	printf_x("Saving gamelist.txt ... ");
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
	printf("ERROR");
}

int Menu_Global_Options()
{
	if (CFG.disable_options) return 0;

	struct discHdr *header = NULL;
	int redraw_cover = 0;

	struct Menu menu;
	menu_init(&menu, 7);

	for (;;) {

		menu.line_count = 0;

		if (gameCnt) {
			header = &gameList[gameSelected];
		} else {
			header = NULL;
		}

		Con_Clear();
		FgColor(CFG.color_header);
		printf_x("Global Options:\n\n");

		MENU_MARK();
		printf("Profile: %d/%d < %s > (%d)\n",
				CFG.current_profile + 1, CFG.num_profiles,
				CFG.profile_names[CFG.current_profile],
				CFG.num_favorite_game);
		MENU_MARK();
		printf("Theme: %2d/%2d < %s >\n",
				cur_theme + 1, num_theme, *CFG.theme ? CFG.theme : "none");
		MENU_MARK();
		printf("Device:      < %s >\n",
				(wbfsDev == WBFS_DEVICE_USB) ? "USB" : "SDHC");
		MENU_MARK();
		printf("Partition:   < %s >\n", CFG.partition);
		MENU_MARK();
		printf("<Download All Missing Covers>\n");
		MENU_MARK();
		printf("<Check For Updates>\n");
		MENU_MARK();
		printf("<Update titles.txt>\n");
		DefaultColor();

		printf("\n");
		printf_h("Press 1 for game options\n");
		printf_h("Press 2 to save global settings\n");
		printf("\n");
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
		if (buttons & WPAD_BUTTON_A) change = +1;

		if (change) {
			switch (menu.current) {
			case 0:
				CHANGE(CFG.current_profile, CFG.num_profiles-1);
				// refresh favorites list
				Switch_Favorites(enable_favorite);
				redraw_cover = 1;
				break;
			case 1:
				CFG_switch_theme(cur_theme + change);
				redraw_cover = 1;
				Cache_Invalidate();
				break;
			case 2:
				Menu_Device();
				return 0;
			case 3:
				Menu_Partition(true);
				return 0;
			case 4:
				Download_All_Covers(change > 0);
				Cache_Invalidate();
				if (header) Gui_DrawCover(header->id);
				break;
			case 5:
				Online_Update();
				break;
			case 6:
				Download_Titles();
				break;
			}
		}
		// HOME button
		if (buttons & WPAD_BUTTON_HOME) {
			Handle_Home(0);
		}
		if (buttons & WPAD_BUTTON_2) {
			printf("\n");
			printf_x("Saving Settings... ");
			__console_flush(0);
			if (CFG_Save_Global_Settings()) {
				printf("OK\n");
				Save_Game_List();
			} else {
				printf("ERROR");
			}
			sleep(1);
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

	if (CFG.admin_lock) {
		if (buttons & WPAD_BUTTON_1) {

			static long long t_start;
			long long t_now;
			unsigned ms_diff = 0;
			bool display_unlock = false;

			Con_Clear();
			t_start = gettime();
			while (!display_unlock && (Wpad_Held(0) & WPAD_BUTTON_1)) {
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
				buttons = WPAD_BUTTON_1;
		}
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

char *get_dev_name(int dev)
{
	switch (dev) {
		case WBFS_DEVICE_USB: return "USB Mass Storage Device";
		case WBFS_DEVICE_SDHC: return "SD/SDHC Card";
	}
	return "Unknown";
}

void print_part(int i, PartList *plist)
{
	partitionEntry *entry = &plist->pentry[i];
	PartInfo *pinfo = &plist->pinfo[i];
	f32 size = entry->size * (plist->sector_size / GB_SIZE);
	if (plist->num == 1) {
		printf("RAW");
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
		if (is_ext) printf("EXTEND/");
		printf("WBFS%d", pinfo->wbfs_i);
		if (WBFS_Mounted() && wbfs_part_idx == pinfo->wbfs_i) {
			if (!is_ext) printf("      ");
			printf("[USED]");
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
			printf("[USED]");
		}
	} else if (pinfo->ntfs_i) {
		printf(" NTFS%d ", pinfo->ntfs_i);
		if (entry->sector == fs_ntfs_sec) {
			//printf(" ntfs:");
			printf(" %s", wbfs_fs_drive);
		}
		if (entry->sector == wbfs_part_lba) {
			printf(" [USED]");
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

	printf_x("%s, please wait...", delete_fs?"Deleting":"Formatting");
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
		printf("\n    ERROR! (ret = %d)\n", ret);
	} else
		printf(" OK!\n");

	// try to remount FAT if was unmounted
	if (re_usb || re_sd) {
		if (re_usb) Fat_MountUSB();
		if (re_sd) Fat_MountSDHC();
		Music_UnPause();
	}

	printf("\n");
	printf("    Press any button to continue...\n");

	// clear button states
	WPAD_Flush(WPAD_CHAN_ALL);

	/* Wait for any button */
	Wpad_WaitButtonsCommon();
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
		printf("[+] ERROR: No partitions found! (ret = %d)\n", ret);
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
	printf_x("Select a partition:\n\n");
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
	printf_h("Press A button to select.\n");
	printf_h("Press B button to go back.\n");
	if (!CFG.disable_format) {
		printf_h("Press + button to format WBFS.\n");
		printf_h("Press - button to delete FS.\n");
		if (invalid_ext >= 0) {
			printf_("\n");
			printf_("NOTE: partition P#%d is type EXTEND but\n", invalid_ext+1);
			printf_("contains a WBFS filesystem. This is an\n");
			printf_("invalid setup. Press 1 to change the\n");
			printf_("partition type from EXTEND to data.\n");
			printf_h("Press 1 button to fix EXTEND/WBFS.\n");
		}
	}

	u32 buttons = Wpad_WaitButtonsCommon();

	menu_move(&menu, buttons);

	// B button
	if (buttons == WPAD_BUTTON_B) {
		if (must_select) {
			if (WBFS_Selected()) return;
			printf_("\nNo partition selected!\n");
			sleep(2);
		} else {
			return;
		}
	}

	// A button
	if (buttons == WPAD_BUTTON_A) {
		i = menu.current;
		entry = &plist.pentry[i];
		pinfo = &plist.pinfo[i];
		int idx = pinfo->wbfs_i;
		int part_fs = PART_FS_WBFS;
		if (pinfo->fat_i) {
			printf("Opening FAT partition...\n");
			idx = pinfo->fat_i;
			part_fs = PART_FS_FAT;
		}
		if (pinfo->ntfs_i) {
			printf("Opening NTFS partition...\n");
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
	if ((buttons == WPAD_BUTTON_MINUS || buttons == WPAD_BUTTON_PLUS)
			&& !CFG.disable_format)
	{
		i = menu.current;
		entry = &plist.pentry[i];
		if (part_valid_data(entry)) {
			char *act = "format";
			if (buttons == WPAD_BUTTON_MINUS) act = "delete";
			printf("\n");
			printf_x("Are you sure you want to %s\n", act);
			printf_( "this partition?\n\n");
			printf_("");
			print_part(i, &plist);
			printf("\n\n");
			if (Menu_Confirm(0)) {
				printf("\n");
				Menu_Format_Partition(entry, buttons == WPAD_BUTTON_MINUS);
				goto rescan;
			}
		}
	}
	// 1 button
	if (buttons == WPAD_BUTTON_1
		&& !CFG.disable_format
		&& invalid_ext >= 0)
	{
		printf("\n");
		printf_x("Are you sure you want to FIX\n");
		printf_( "this partition?\n\n");
		printf_("");
		print_part(invalid_ext, &plist);
		printf("\n\n");
		if (Menu_Confirm(0)) {
			printf("\n");
			printf_("Fixing EXTEND partition...");
			ret = Partition_FixEXT(wbfsDev, invalid_ext);
			printf("%s\n", ret ? "FAIL" : "OK");
			printf_("Press any button...");
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
	restart:

	/* Ask user for device */
	for (;;) {
		char *devname = "Unknown!";

		/* Set device name */
		devname = get_dev_name(wbfsDev);

		//Enable the console
		Gui_Console_Enable();

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
		if (buttons & WPAD_BUTTON_A)
			break;

		// B button
		if (buttons & WPAD_BUTTON_B) {
		   	if (!first_time && WBFS_Selected()) {
				wbfsDev = save_wbfsDev;
				return;
			}
		}
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
		//Enable the console
		Gui_Console_Enable();
		printf_("ERROR! (ret = %d)\n", ret);
		printf_("Make sure USB port 0 is used!\n");
		printf_("(The one nearest to the edge)\n");

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

		printf_x("WARNING:\n\n");

		printf_("Partition: %s not found!\n", CFG.partition);
		printf_("Select a different partition\n");
		if (!CFG.disable_format) {
		printf_("or format a WBFS partition.\n");
		}
		printf("\n");

		printf_h("Press A button to select a partition.\n");
		printf_h("Press B button to change device.\n");
		printf_h("Press Home button to exit.\n\n");

		// clear button states
		WPAD_Flush(WPAD_CHAN_ALL);

		/* Wait for user answer */
		for (;;) {
			u32 buttons = Wpad_WaitButtonsCommon();
			if (buttons == WPAD_BUTTON_A) break;
			if (buttons == WPAD_BUTTON_B) goto restart;
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
	printf_("Reading BCA...\n\n");
	ret = WDVD_Read_Disc_BCA(BCA_Data);
	hex_dump3(BCA_Data, 64);
	printf_("\n");
	if (ret) {
		printf_("ERROR reading BCA!\n\n");
		goto out;
	}
	// save
	snprintf(D_S(fname), "%s/%.6s.bca", USBLOADER_PATH, (char*)id);
	if (!Menu_Confirm("save")) return;
	printf("\n");
	printf_("Writing: %s\n\n", fname);
	FILE *f = fopen(fname, "wb");
	if (!f) {
		printf_("ERROR writing BCA!\n\n");
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
	printf_x("Install game\n\n");
	DefaultColor();

	/* Disable WBFS mode */
	Disc_SetWBFS(0, NULL);

	// Wait for disc
	//printf_x("Checking game disc...");
	u32 cover = 0;
	ret = WDVD_GetCoverStatus(&cover);
	if (ret < 0) {
		err1:
		printf("\n");
		printf_("ERROR! (ret = %d)\n", ret);
		goto out;
	}
	if (!(cover & 0x2)) {
		printf_("Please insert a game disc...\n\n");
		printf_h("Press B to go back\n\n");
		do {
			ret = WDVD_GetCoverStatus(&cover);
			if (ret < 0) goto err1;
			u32 buttons = Wpad_GetButtons();
			if (buttons & WPAD_BUTTON_B) return;
			VIDEO_WaitVSync();
		} while(!(cover & 0x2));
	}

	// Open disc
	printf_x("Opening DVD disc...");

	ret = Disc_Open();
	if (ret < 0) {
		printf("\n");
		printf_("ERROR! (ret = %d)\n", ret);
		goto out;
	} else
		printf(" OK!\n");

	/* Check disc */
	ret = Disc_IsWii();
	if (ret < 0) {
		printf_x("ERROR: Not a Wii disc!!\n");
		goto out;
	}

	/* Read header */
	Disc_ReadHeader(&header);
	u64 comp_size = 0, real_size = 0;
	WBFS_DVD_Size(&comp_size, &real_size);

	printf("\n");
	__Menu_PrintInfo2(&header, comp_size, real_size);

	// Disk free space
	f32 free, used, total;
	WBFS_DiskSpace(&used, &free);
	total = used + free;
	//printf_x("WBFS: %.1fGB free of %.1fGB\n\n", free, total);
	printf_x("%s: %.1fGB free of %.1fGB\n\n", CFG.partition, free, total);

	bench_io();

	if (check_dual_layer(real_size, NULL)) print_dual_layer_note();

	/* Check if game is already installed */
	int way_out = 0;
	ret = WBFS_CheckGame(header.id);
	if (ret) {
		printf_x("ERROR: Game already installed!!\n\n");
		//goto out;
		way_out = 1;
	}
	// require +128kb for operating safety
	if ((f32)comp_size + (f32)128*1024 >= free * GB_SIZE) {
		printf_x("ERROR: not enough free space!!\n\n");
		//goto out;
		way_out = 1;
	}

	// get confirmation
	retry:
	if (!way_out)
		printf_h("Press A button to continue.\n");
	printf_h("Press 1 button to dump BCA.\n");
	printf_h("Press B button to go back.\n");
	DefaultColor();
	for (;;) {
		u32 buttons = Wpad_WaitButtonsCommon();
		if (!way_out)
			if (buttons & WPAD_BUTTON_A) break;
		if (buttons & WPAD_BUTTON_B) return;
		if (buttons & WPAD_BUTTON_1) {
			Menu_DumpBCA(header.id);
			if (way_out) return;
			Con_Clear();
			printf_x("Install game\n\n");
			__Menu_PrintInfo2(&header, comp_size, real_size);
			printf_x("WBFS: %.1fGB free of %.1fGB\n\n", free, total);
			goto retry;
		}
	}

	printf_x("Installing game, please wait...\n\n");

	// Pause the music
	Music_Pause();

	/* Install game */
	ret = WBFS_AddGame();

	// UnPause the music
	Music_UnPause();

	if (ret < 0) {
		printf_x("Installation ERROR! (ret = %d)\n", ret);
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
	printf_h("Press button A to eject DVD.\n");
	printf_h("Press any button to continue...\n\n");

	/* Wait for any button */
	u32 buttons = Wpad_WaitButtons();

	/* Turn off the Slot Light */
	wiilight(0);

	// button A
	if (buttons & WPAD_BUTTON_A) {
		printf_("Ejecting DVD...\n");
		WDVD_Eject();
		sleep(1);
	}
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
	printf("\nRunning benchmark, please wait\n");
	__console_flush(0);

	used = calloc(n_wii_sec_per_disc,1);
	d = wd_open_disc(__WBFS_ReadDVD, NULL);
	if (!d) { printf("unable to open wii disc"); goto out; }
	wd_build_disc_usage(d,ALL_PARTITIONS,used);
	wd_close_disc(d);
	d = 0;
	printf("linear...\n"); __console_flush(0);
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
	printf("\nlinear read speed: %.2f mb/s\n",
			(float)size / ms * 1000 / 1024 / 1024);

	count = 0;
	printf("random...\n"); __console_flush(0);
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
	printf("\nrandom read speed: %.2f mb/s\n",
			(float)size / ms * 1000 / 1024 / 1024);
out:
	printf("\nPress any button\n");
	Wpad_WaitButtonsCommon();
	//exit(0);
}


void Menu_Boot(void)
{
	struct discHdr *header = NULL;

	s32 ret;
	struct Game_CFG_2 *game_cfg = NULL;
	u64 comp_size = 0, real_size = 0;

	/* No game list */
	if (!gameCnt)
		return;

	/* Selected game */
	header = &gameList[gameSelected];

	game_cfg = CFG_find_game(header->id);

	/* Clear console */
	if (!CFG.direct_launch) {
		Con_Clear();
	}

	// Get game size
	WBFS_GameSize2(header->id, &comp_size, &real_size);
	bool dl_warn = check_dual_layer(real_size, game_cfg);
	bool can_skip = !CFG.confirm_start && !dl_warn
		&& check_device(game_cfg, false);

	if (can_skip) {
		printf("\n");
		/* Show game info */
		__Menu_PrintInfo(header);
			goto skip_confirm;
	}

	Gui_Console_Enable();
	FgColor(CFG.color_header);
	printf_x("Start this game?\n\n");
	DefaultColor();

	/* Show game info */
	__Menu_PrintInfo(header);

	check_device(game_cfg, true);
	if (dl_warn) print_dual_layer_note();

	//ret = get_frag_list(header->id);
	//if (ret) goto out;

	printf_h("Press A button to continue.\n");
	printf_h("Press B button to go back.\n");
	printf_h("Press 1 button for options\n\n");
	__console_flush(0);
	
	// play banner sound
	SoundInfo snd;
	memset(&snd, 0, sizeof(snd));
	WBFS_BannerSound(header->id, &snd);
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
		if (buttons & WPAD_BUTTON_A) break;
		if (buttons & WPAD_BUTTON_B) break;
		if (buttons & WPAD_BUTTON_1) break;
		if (buttons & WPAD_BUTTON_HOME) break;
	}
	// stop banner sound, resume mp3
	if (snd.dsp_data) {
		SND_StopVoice(1);
		SAFE_FREE(snd.dsp_data);
		if (buttons & WPAD_BUTTON_A) {
			SND_ChangeVolumeVoice(0, 0, 0);
		}
		SND_PauseVoice(0, 0);
	}

	if (buttons & WPAD_BUTTON_B) return;
	if (buttons & WPAD_BUTTON_HOME) {
		Handle_Home(0);
		return;
	}
	if (buttons & WPAD_BUTTON_1) {
		Menu_Options();
		return;
	}
	// A button: continue to boot

	skip_confirm:

	if (game_cfg) {
		CFG.game = game_cfg->curr;
		cfg_ios_set_idx(CFG.game.ios_idx);
		//dbg_printf("set ios: %d idx: %d\n", CFG.ios, CFG.game.ios_idx);
	}

	printf("\n");
	printf_x("Booting Wii game, please wait...\n\n");

	// If fat, open wbfs disc and verfy id as a consistency check
	if (wbfs_part_fs) {
		wbfs_disc_t *d = WBFS_OpenDisc(header->id);
		if (!d) {
			printf_("ERROR: opening %.6s\n", header->id);
			goto out;
		}
		WBFS_CloseDisc(d);
		if (CFG.game.ios_idx != CFG_IOS_222_MLOAD &&
			CFG.game.ios_idx != CFG_IOS_223_MLOAD)
		{
			printf("Switching to IOS222 for FAT support.\n");
			CFG.game.ios_idx = CFG_IOS_222_MLOAD;
			cfg_ios_set_idx(CFG.game.ios_idx);
		}
	}

	// load stuff before ios reloads & services close
	if (CFG.game.ocarina) {
		ocarina_load_code(header->id);
	}
	load_wip_patches(header->id);
	load_bca_data(header->id);

	ret = get_frag_list(header->id);
	if (ret) {
		printf_("ERROR: get_frag_list: %d\n", ret);
		goto out;
	}

	// stop services (music, gui)
	Services_Close();

	if (CFG.game.alt_dol != 1) {
		// unless we're loading alt.dol from sd
		// unmount everything
		Fat_UnmountAll();
	}

	ret = ReloadIOS(1, 1);
	if (ret < 0) goto out;
	Block_IOS_Reload();

	// verify IOS version
	warn_ios_bugs();
	
	ret = set_frag_list(header->id);
	if (ret) {
		printf_("ERROR: set_frag_list: %d\n", ret);
		goto out;
	}

	//dbg_time1();

	/* Set WBFS mode */
	Disc_SetWBFS(wbfsDev, header->id);

	/* Open disc */
	ret = Disc_Open();

	//dbg_time2("\ndisc open");
	//Wpad_WaitButtonsCommon();
	bench_io();

	if (ret < 0) {
		printf("    ERROR: Could not open game! (ret = %d)\n", ret);
		goto out;
	}

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
	ret = Disc_WiiBoot();

	printf("    Returned! (ret = %d)\n", ret);

out:
	printf("\n");
	printf("    Press any button to exit...\n");

	/* Wait for button */
	Wpad_WaitButtonsCommon();
	exit(0);
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
	printf("Configurable Loader %s\n\n", CFG_VERSION);
	//sleep(1);
	for (i=0; i<gameCnt; i++) {
		if (strncmp(CFG.launch_discid, (char*)gameList[i].id, 6) == 0) {
			gameSelected = i;
			Menu_Boot();
			goto out;
		}
	}
	Gui_Console_Enable();
	printf("Auto-start game: %.6s not found!\n", CFG.launch_discid);
	printf("Press any button...\n");
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

int menu_mark(struct Menu *m)
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
	return m->line_count++;
}


void printf_(char *fmt, ...)
{
	va_list argp;
	printf("%s", CFG.menu_plus_s);
	va_start(argp, fmt);
	vprintf(fmt, argp);
	va_end(argp);
}

void printf_x(char *fmt, ...)
{
	va_list argp;
	printf("%s", CFG.menu_plus);
	va_start(argp, fmt);
	vprintf(fmt, argp);
	va_end(argp);
}

void printf_h(char *fmt, ...)
{
	va_list argp;
	DefaultColor();
	FgColor(CFG.color_help);
	printf("%s", CFG.menu_plus_s);
	va_start(argp, fmt);
	vprintf(fmt, argp);
	va_end(argp);
	DefaultColor();
}

int Menu_PrintWait()
{
	printf_h("Press any button to continue...\n");
	return Wpad_WaitButtonsCommon();
}

bool Menu_Confirm(char *msg)
{
	printf_h("Press A button to %s.\n", msg ? msg : "continue");
	printf_h("Press B button to go back.\n");
	WPAD_Flush(WPAD_CHAN_ALL);
	for (;;) {
		u32 buttons = Wpad_WaitButtonsCommon();
		if (buttons == WPAD_BUTTON_A) return true;
		if (buttons == WPAD_BUTTON_B) return false;
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

