#include <gccore.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include "menu.h"
#include "xml.h"
#include "cfg.h"
#include "video.h"
#include "gui.h"
#include "libwbfs/libwbfs.h"
#include "wbfs_fat.h"
#include "sort.h"
#include "wpad.h"

extern struct discHdr *all_gameList;
extern struct discHdr *gameList;
extern struct discHdr *filter_gameList;
extern bool enable_favorite;
extern s32 filter_gameCnt;

s32 filter_index = -1;
s32 filter_type = -1;
s32 sort_index = -1;
bool sort_desc = 0;

s32 __sort_play_date(struct discHdr * hdr1, struct discHdr * hdr2, bool desc);
s32 __sort_install_date(struct discHdr * hdr1, struct discHdr * hdr2, bool desc);
s32 __sort_title(struct discHdr * hdr1, struct discHdr * hdr2, bool desc);
s32 __sort_releasedate(struct discHdr * hdr1, struct discHdr * hdr2, bool desc);
s32 __sort_players(struct discHdr * hdr1, struct discHdr * hdr2, bool desc);
s32 __sort_wifiplayers(struct discHdr * hdr1, struct discHdr * hdr2, bool desc);
s32 __sort_pub(struct discHdr * hdr1, struct discHdr * hdr2, bool desc);
s32 __sort_dev(struct discHdr * hdr1, struct discHdr * hdr2, bool desc);
s32 __sort_players_asc(const void *a, const void *b);
s32 __sort_players_desc(const void *a, const void *b);
s32 __sort_wifiplayers_asc(const void *a, const void *b);
s32 __sort_wifiplayers_desc(const void *a, const void *b);
s32 __sort_pub_asc(const void *a, const void *b);
s32 __sort_pub_desc(const void *a, const void *b);
s32 __sort_dev_asc(const void *a, const void *b);
s32 __sort_dev_desc(const void *a, const void *b);
s32 __sort_releasedate_asc(const void *a, const void *b);
s32 __sort_releasedate_desc(const void *a, const void *b);
s32 __sort_title_asc(const void *a, const void *b);
s32 __sort_title_desc(const void *a, const void *b);
s32 __sort_play_count_asc(const void *a, const void *b);
s32 __sort_play_count_desc(const void *a, const void *b);
s32 __sort_install_date_asc(const void *a, const void *b);
s32 __sort_install_date_desc(const void *a, const void *b);
s32 __sort_play_date_asc(const void *a, const void *b);
s32 __sort_play_date_desc(const void *a, const void *b);

#define featureCnt 4
#define accessoryCnt 16
#define genreCnt 14
#define sortCnt 9

static char featureTypes[featureCnt][2][25] = {
	{"online", "Online Content"},
	{"download", "Downloadable Content"},
	{"score", "Online Score List"},
	{"nintendods", "Nintendo DS Connectivity"}
};

static char accessoryTypes[accessoryCnt][2][20] = {
	{"wiimote", "Wiimote"},
	{"nunchuk", "Nunchuk"},
	{"motionplus", "Motion+"},
	{"gamecube", "Gamecube"},
	{"nintendods", "Nintendo DS"},
	{"classiccontroller", "Classic Controller"},
	{"wheel", "Wheel"},
	{"zapper", "Zapper"},
	{"balanceboard", "Balance Board"},
	{"wiispeak", "Wii Speak"},
	{"microphone", "Microphone"},
	{"guitar", "Guitar"},
	{"drums", "Drums"},
	{"dancepad", "Dance Pad"},
	{"keyboard", "Keyboard"},
	{"vitalitysensor", "Vitality Sensor"}
};

static char genreTypes[genreCnt][2][20] = {
	{"action", "Action"},
	{"adventure", "Adventure"},
	{"sport", "Sports"},
	{"racing", "Racing"},
	{"rhythm", "Rhythm"},
	{"simulation", "Simulation"},
	{"platformer", "Platformer"},
	{"party", "Party"},
	{"music", "Music"},
	{"puzzle", "Puzzle"},
	{"fighting", "Fighting"},
	{"rpg", "RPG"},
	{"shooter", "Shooter"},
	{"strategy", "Strategy"}
};

static struct Sorts sortTypes[sortCnt] = {
	{"title", "Title", __sort_title_asc,	__sort_title_desc},
	{"players", "Number of Players", __sort_players_asc, __sort_players_desc},
	{"online_players", "Number of Online Players", __sort_wifiplayers_asc, __sort_wifiplayers_desc},
	{"publisher", "Publisher", __sort_pub_asc, __sort_pub_desc},
	{"developer", "Developer", __sort_dev_asc, __sort_dev_desc},
	{"release", "Release Date", __sort_releasedate_asc, __sort_releasedate_desc},
	{"play_count", "Play Count", __sort_play_count_asc, __sort_play_count_desc},
	{"play_date", "Last Play Date", __sort_play_date_asc, __sort_play_date_desc},
	{"install", "Install Date", __sort_install_date_asc, __sort_install_date_desc}
};

void __set_default_sort() {

	char tmp[20];
	bool dsc = 0;
	int n = 0;
	strncpy(tmp, CFG.sort, sizeof(tmp));
	char * ptr = strchr(tmp, '-');
	if (ptr) {
		*ptr = '\0';
		dsc = (*(ptr+1) == 'd' || *(ptr+1) == 'D');
	}
	for (; n<sortCnt; n++) {
		if (!strncmp(sortTypes[n].cfg_val, tmp, strlen(tmp))) {
			sort_index = n;
			if (!dsc) {
				default_sort_function = (*sortTypes[sort_index].sortAsc);
				sort_desc = 0;
			} else {
				default_sort_function = (*sortTypes[sort_index].sortDsc);
				sort_desc = 1;
			}
			return;
		}
	}
	sort_index = 0;
	default_sort_function =  (*sortTypes[sort_index].sortAsc);
}

int filter_features(struct discHdr *list, int cnt, char *feature, bool requiredOnly)
{
	int i;
	for (i=0; i<cnt;) {
		if (!hasFeature(feature, list[i].id)) {
			memcpy(list+i, list+i+1, (cnt-i-1) * sizeof(struct discHdr));
			cnt--;
		} else {
			i++;
		}
	}
	return cnt;
}

int filter_online(struct discHdr *list, int cnt, char * name, bool notused)
{
	int i;
	for (i=0; i<cnt;) {
		int id = getIndexFromId(list[i].id);
		if (id < 0 || game_info[id].wifiplayers < 1) {
			memcpy(list+i, list+i+1, (cnt-i-1) * sizeof(struct discHdr));
			cnt--;
		} else {
			i++;
		}
	}
	return cnt;
}

int filter_controller(struct discHdr *list, int cnt, char *controller, bool requiredOnly)
{
	int i;
	for (i=0; i<cnt;) {
		if (getControllerTypes(controller, list[i].id) < (requiredOnly ? 1 : 0)) {
			memcpy(list+i, list+i+1, (cnt-i-1) * sizeof(struct discHdr));
			cnt--;
		} else {
			i++;
		}
	}
	return cnt;
}

int filter_genre(struct discHdr *list, int cnt, char *genre, bool notused)
{
	int i;
	for (i=0; i<cnt;) {
		if (!hasGenre(genre, list[i].id)) {
			memcpy(list+i, list+i+1, (cnt-i-1) * sizeof(struct discHdr));
			cnt--;
		} else {
			i++;
		}
	}
	return cnt;
}

int filter_play_count(struct discHdr *list, int cnt, char *ignore, bool notused)
{
	int i;
	for (i=0; i<cnt;) {
		if (getPlayCount(list[i].id) > 0) {
			memcpy(list+i, list+i+1, (cnt-i-1) * sizeof(struct discHdr));
			cnt--;
		} else {
			i++;
		}
	}
	return cnt;
}

int filter_games(int (*filter) (struct discHdr *, int, char *, bool), char * name, bool num)
{
	int i, len;
	int ret = 0;
	u8 *id = NULL;
	// filter
	if (filter_gameList) {
		len = sizeof(struct discHdr) * all_gameCnt;
		memcpy(filter_gameList, all_gameList, len);
		filter_gameCnt = filter(filter_gameList, all_gameCnt, name, num);
	}
	if (gameSelected < gameCnt) {
		id = gameList[gameSelected].id;
	}
	if (filter_gameCnt > 0) {
		gameList = filter_gameList;
		gameCnt = filter_gameCnt;
		ret = 1;
	} else {
		Con_Clear();
		FgColor(CFG.color_header);
		printf("\n");
		printf("No games found!\nLoading previous game list...\n");
		DefaultColor();
		__console_flush(0);
		sleep(1);
		ret = -1;
	}
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
	return ret;
}

void showAllGames(void)
{
	int i;
	u8 *id = NULL;
	if (gameSelected < gameCnt) {
		id = gameList[gameSelected].id;
	}
	gameList = all_gameList;
	gameCnt = all_gameCnt;
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

s32 __sort_play_date(struct discHdr * hdr1, struct discHdr * hdr2, bool desc)
{
	int ret;
	if (desc)
		ret = (getLastPlay(hdr2->id) - getLastPlay(hdr1->id));
	else
		ret = (getLastPlay(hdr1->id) - getLastPlay(hdr2->id));
	if (ret == 0) ret = __sort_title(hdr1, hdr2, desc);
	return ret;
}

s32 __sort_install_date(struct discHdr * hdr1, struct discHdr * hdr2, bool desc)
{
	int ret;
	char fname1[1024];
	char fname2[1024];
	struct stat fileStat1;
	struct stat fileStat2; 
	WBFS_FAT_find_fname(hdr1->id, fname1, sizeof(fname1));
	WBFS_FAT_find_fname(hdr2->id, fname2, sizeof(fname2));
	int err1 = stat(fname1, &fileStat1); 
	int err2 = stat(fname2, &fileStat2);
	if (err1 != 0 || err2 != 0)
		ret = 0;
	else if (desc)
		ret = (fileStat2.st_ctime - fileStat1.st_ctime);
	else
		ret = (fileStat1.st_ctime - fileStat2.st_ctime);
	if (ret == 0) ret = __sort_title(hdr1, hdr2, desc);
	return ret;
}

s32 __sort_title(struct discHdr * hdr1, struct discHdr * hdr2, bool desc)
{
	char *title1 = get_title(hdr1);
	char *title2 = get_title(hdr2);
	title1 = skip_sort_ignore(title1);
	title2 = skip_sort_ignore(title2);
	if (desc)
		return mbs_coll(title2, title1);
	else
		return mbs_coll(title1, title2);
}

s32 __sort_releasedate(struct discHdr * hdr1, struct discHdr * hdr2, bool desc)
{
	int ret;
	int id1 = getIndexFromId(hdr1->id);
	int id2 = getIndexFromId(hdr2->id);
	if (id1 < 0 || id2 < 0) {
		ret = 0;
	} else {
		int date1 = (game_info[id1].year * 10000) + (game_info[id1].month * 100) + (game_info[id1].day);
		int date2 = (game_info[id2].year * 10000) + (game_info[id2].month * 100) + (game_info[id2].day);
		if (desc)
			ret = (date2 - date1);
		else
			ret = (date1 - date2);
	}
	if (ret == 0) ret = __sort_title(hdr1, hdr2, desc);
	return ret;
}

s32 __sort_players(struct discHdr * hdr1, struct discHdr * hdr2, bool desc)
{
	int ret;
	int id1 = getIndexFromId(hdr1->id);
	int id2 = getIndexFromId(hdr2->id);
	if (id1 < 0 || id2 < 0)
		ret = 0;
	else if (desc)
		ret = (game_info[id2].players - game_info[id1].players);
	else
		ret = (game_info[id1].players - game_info[id2].players);
	if (ret == 0) ret = __sort_title(hdr1, hdr2, desc);
	return ret;
}

s32 __sort_wifiplayers(struct discHdr * hdr1, struct discHdr * hdr2, bool desc)
{
	int ret;
	int id1 = getIndexFromId(hdr1->id);
	int id2 = getIndexFromId(hdr2->id);
	if (id1 < 0 || id2 < 0)
		ret = 0;
	else if (desc)
		ret = (game_info[id2].wifiplayers - game_info[id1].wifiplayers);
	else
		ret = (game_info[id1].wifiplayers - game_info[id2].wifiplayers);
	if (ret == 0) ret = __sort_title(hdr1, hdr2, desc);
	return ret;
}

s32 __sort_pub(struct discHdr * hdr1, struct discHdr * hdr2, bool desc)
{
	int ret;
	int id1 = getIndexFromId(hdr1->id);
	int id2 = getIndexFromId(hdr2->id);
	if (id1 < 0 || id2 < 0)
		ret = 0;
	else if (desc)
		ret = mbs_coll(game_info[id2].publisher, game_info[id1].publisher);
	else
		ret = mbs_coll(game_info[id1].publisher, game_info[id2].publisher);
	if (ret == 0) ret = __sort_title(hdr1, hdr2, desc);
	return ret;
}

s32 __sort_dev(struct discHdr * hdr1, struct discHdr * hdr2, bool desc)
{
	int ret;
	int id1 = getIndexFromId(hdr1->id);
	int id2 = getIndexFromId(hdr2->id);
	if (id1 < 0 || id2 < 0)
		ret = 0;
	else if (desc)
		ret = mbs_coll(game_info[id2].developer, game_info[id1].developer);
	else
		ret = mbs_coll(game_info[id1].developer, game_info[id2].developer);
	if (ret == 0) ret = __sort_title(hdr1, hdr2, desc);
	return ret;
}

s32 __sort_play_count(struct discHdr * hdr1, struct discHdr * hdr2, bool desc)
{
	int ret;
	if (desc)
		ret = (getPlayCount(hdr2->id) - getPlayCount(hdr1->id));
	else
		ret = (getPlayCount(hdr1->id) - getPlayCount(hdr2->id));
	if (ret == 0) ret = __sort_title(hdr1, hdr2, desc);
	return ret;
}

s32 __sort_players_asc(const void *a, const void *b)
{
	return __sort_players((struct discHdr *)a, (struct discHdr *)b, 0);
}

s32 __sort_players_desc(const void *a, const void *b)
{
	return __sort_players((struct discHdr *)a, (struct discHdr *)b, 1);
}

s32 __sort_wifiplayers_asc(const void *a, const void *b)
{
	return __sort_wifiplayers((struct discHdr *)a, (struct discHdr *)b, 0);
}

s32 __sort_wifiplayers_desc(const void *a, const void *b)
{
	return __sort_wifiplayers((struct discHdr *)a, (struct discHdr *)b, 1);
}

s32 __sort_pub_asc(const void *a, const void *b)
{
	return __sort_pub((struct discHdr *)a, (struct discHdr *)b, 0);
}

s32 __sort_pub_desc(const void *a, const void *b)
{
	return __sort_pub((struct discHdr *)a, (struct discHdr *)b, 1);
}

s32 __sort_dev_asc(const void *a, const void *b)
{
	return __sort_dev((struct discHdr *)a, (struct discHdr *)b, 0);
}

s32 __sort_dev_desc(const void *a, const void *b)
{
	return __sort_dev((struct discHdr *)a, (struct discHdr *)b, 1);
}

s32 __sort_releasedate_asc(const void *a, const void *b)
{
	return __sort_releasedate((struct discHdr *)a, (struct discHdr *)b, 0);
}

s32 __sort_releasedate_desc(const void *a, const void *b)
{
	return __sort_releasedate((struct discHdr *)a, (struct discHdr *)b, 1);
}

s32 __sort_play_count_asc(const void *a, const void *b)
{
	return __sort_play_count((struct discHdr *)a, (struct discHdr *)b, 0);
}

s32 __sort_play_count_desc(const void *a, const void *b)
{
	return __sort_play_count((struct discHdr *)a, (struct discHdr *)b, 1);
}

s32 __sort_title_asc(const void *a, const void *b)
{
	return __sort_title((struct discHdr *)a, (struct discHdr *)b, 0);
}

s32 __sort_title_desc(const void *a, const void *b)
{
	return __sort_title((struct discHdr *)a, (struct discHdr *)b, 1);
}

s32 __sort_install_date_asc(const void *a, const void *b)
{
	return __sort_install_date((struct discHdr *)a, (struct discHdr *)b, 0);
}

s32 __sort_install_date_desc(const void *a, const void *b)
{
	return __sort_install_date((struct discHdr *)a, (struct discHdr *)b, 1);
}

s32 __sort_play_date_asc(const void *a, const void *b)
{
	return __sort_play_date((struct discHdr *)a, (struct discHdr *)b, 0);
}

s32 __sort_play_date_desc(const void *a, const void *b)
{
	return __sort_play_date((struct discHdr *)a, (struct discHdr *)b, 1);
}

void sortList(int (*sortFunc) (const void *, const void *))
{
	qsort(gameList, gameCnt, sizeof(struct discHdr), sortFunc);
	// scroll start list
	__Menu_ScrollStartList();
}

int Menu_Filter()
{
	struct discHdr *header = NULL;
	int redraw_cover = 0;
	struct Menu menu;
	menu_init(&menu, genreCnt+5);
	for (;;) {

		menu.line_count = 0;

		if (gameCnt) {
			header = &gameList[gameSelected];
		} else {
			header = NULL;
		}
		int n;
		Con_Clear();
		FgColor(CFG.color_header);
		printf_x("Filter by Genre:\n\n");
		MENU_MARK();
		printf("<Filter by Controller>\n");
		MENU_MARK();
		printf("<Filter by Online Features>\n");
		MENU_MARK();
		printf("%s All Games\n", ((filter_type == -1) ? "*" : " "));
		MENU_MARK();
		printf("%s Online Play\n", ((filter_type == 0) ? "*": " "));
		MENU_MARK();
		printf("%s Unplayed Games\n", ((filter_type == 1) ? "*": " "));
		for (n=0;n<genreCnt;n++) {
			MENU_MARK();
			printf("%s %s\n", ((filter_index == n && filter_type == 2) ? "*": " "), genreTypes[n][1]);
		}
		
		DefaultColor();

		printf("\n");
		printf_h("Press A to select filter type\n");
		printf("\n");
		__console_flush(0);

		if (redraw_cover) {
			if (header) Gui_DrawCover(header->id);
			redraw_cover = 0;
		}
		
		u32 buttons = Wpad_WaitButtonsRpt();
		menu_move(&menu, buttons);

		int change = 0;
		if (buttons & WPAD_BUTTON_LEFT) change = -1;
		if (buttons & WPAD_BUTTON_RIGHT || buttons & WPAD_BUTTON_A || buttons & WPAD_BUTTON_2) change = +1;

		if (change) {
			if (0 == menu.current) {
				Menu_Filter2();
				redraw_cover = 1;
				break;
			} else if (1 == menu.current) {
				Menu_Filter3();
				redraw_cover = 1;
				break;
			} else if (2 == menu.current) {
				showAllGames();
				filter_type = -1;
				redraw_cover = 1;
			} else if (3 == menu.current) {
				redraw_cover = 1;
				if (filter_games(filter_online, "", 0) > -1) {
					filter_type = 0;
				}
			} else if (4 == menu.current) {
				redraw_cover = 1;
				if (filter_games(filter_play_count, "", 0) > -1) {
					filter_type = 1;
				}
			}
			for (n=0;n<genreCnt;n++) {
				if (5+n == menu.current) {
					redraw_cover = 1;
					if (filter_games(filter_genre, genreTypes[n][0], 0) > -1) {
						filter_type = 2;
						filter_index = n;
					}
					break;
				}
			}
		}
		
		// HOME button
		if (buttons & WPAD_BUTTON_HOME) {
			Handle_Home(0);
		}
		if (buttons & WPAD_BUTTON_B) break;
	}
	return 0;
}

int Menu_Filter2()
{
	struct discHdr *header = NULL;
	int redraw_cover = 0;
	struct Menu menu;
	menu_init(&menu, accessoryCnt+2);
	for (;;) {

		menu.line_count = 0;

		if (gameCnt) {
			header = &gameList[gameSelected];
		} else {
			header = NULL;
		}
		int n;
		Con_Clear();
		FgColor(CFG.color_header);
		printf_x("Filter by Controller:\n\n");
		MENU_MARK();
		printf("<Filter by Genre>\n");
		MENU_MARK();
		printf("<Filter by Online Features>\n");
		for (n=0; n<accessoryCnt; n++) {
			MENU_MARK();
			printf("%s %s\n", ((filter_index == n && filter_type == 4) ? "*": " "), accessoryTypes[n][1]);
		}
		DefaultColor();

		printf("\n");
		printf_h("Press A to select filter type\n");
		printf("\n");
		__console_flush(0);

		if (redraw_cover) {
			if (header) Gui_DrawCover(header->id);
			redraw_cover = 0;
		}
		
		u32 buttons = Wpad_WaitButtonsRpt();
		menu_move(&menu, buttons);

		int change = 0;
		if (buttons & WPAD_BUTTON_LEFT) change = -1;
		if (buttons & WPAD_BUTTON_RIGHT || buttons & WPAD_BUTTON_A || buttons & WPAD_BUTTON_2) change = +1;

		if (change) {
			if (0 == menu.current) {
				Menu_Filter();
				redraw_cover = 1;
				goto end;
			} else if (1 == menu.current) {
				Menu_Filter3();
				redraw_cover = 1;
				goto end;
			}
			for (n=0;n<accessoryCnt;n++) {
				if (n+2 == menu.current) {
					redraw_cover = 1;
					if (filter_games(filter_controller, accessoryTypes[n][0], 0) > -1) {
						filter_type = 4;
						filter_index = n;
					}
					break;
				}
			}
		}
		
		// HOME button
		if (buttons & WPAD_BUTTON_HOME) {
			Handle_Home(0);
		}
		if (buttons & WPAD_BUTTON_B) break;
	}
	end:
	return 0;
}

int Menu_Filter3()
{
	struct discHdr *header = NULL;
	int redraw_cover = 0;
	struct Menu menu;
	menu_init(&menu, featureCnt+2);
	for (;;) {

		menu.line_count = 0;

		if (gameCnt) {
			header = &gameList[gameSelected];
		} else {
			header = NULL;
		}
		int n;
		Con_Clear();
		FgColor(CFG.color_header);
		printf_x("Filter by Online Features:\n\n");
		MENU_MARK();
		printf("<Filter by Genre>\n");
		MENU_MARK();
		printf("<Filter by Controller>\n");		
		for (n=0;n<featureCnt;n++) {
			MENU_MARK();
			printf("%s %s\n", ((filter_index == n && filter_type == 3) ? "*": " "), featureTypes[n][1]);
		}
		DefaultColor();

		printf("\n");
		printf_h("Press A to select filter type\n");
		printf("\n");
		__console_flush(0);

		if (redraw_cover) {
			if (header) Gui_DrawCover(header->id);
			redraw_cover = 0;
		}
		
		u32 buttons = Wpad_WaitButtonsRpt();
		menu_move(&menu, buttons);

		int change = 0;
		if (buttons & WPAD_BUTTON_LEFT) change = -1;
		if (buttons & WPAD_BUTTON_RIGHT || buttons & WPAD_BUTTON_A || buttons & WPAD_BUTTON_2) change = +1;

		if (change) {
			if (0 == menu.current) {
				Menu_Filter();
				redraw_cover = 1;
				goto end;
			} else if (1 == menu.current) {
				Menu_Filter2();
				redraw_cover = 1;
				goto end;
			}
			for (n=0;n<featureCnt;n++) {
				if (2+n == menu.current) {
					redraw_cover = 1;
					if (filter_games(filter_features, featureTypes[n][0], 0) > -1) {
						filter_type = 3;
						filter_index = n;
					}
					break;
				}
			}
		}
		
		// HOME button
		if (buttons & WPAD_BUTTON_HOME) {
			Handle_Home(0);
		}
		if (buttons & WPAD_BUTTON_B) break;
	}
	end:
	return 0;
}

int Menu_Sort()
{
	struct discHdr *header = NULL;
	int redraw_cover = 0;
	int n = 0;
	bool first_run = 1;
	bool descend[sortCnt];
	for (;n<sortCnt;n++)
		descend[n] = 0;
	struct Menu menu;
	menu_init(&menu, sortCnt);
	for (;;) {

		menu.line_count = 0;

		if (gameCnt) {
			header = &gameList[gameSelected];
		} else {
			header = NULL;
		}
		Con_Clear();
		FgColor(CFG.color_header);
		printf_x("Choose a sorting method:\n\n");
		for (n=0; n<sortCnt; n++) {
			if (sort_index == n && sort_desc && first_run) {
				first_run = 0;
				descend[n] = 1;
			}
			MENU_MARK();
			printf("%s %s", (sort_index == n ? "*": " "), sortTypes[n].name);
			printf("%*s\n", (MAX_CHARACTERS - (strlen(sortTypes[n].name) + 5)), (!descend[n] ? "< ASC  >" : "< DESC >"));
		}
		DefaultColor();

		printf("\n");
		printf_h("Press A to select sorting method\n");
		printf("\n");
		__console_flush(0);

		if (redraw_cover) {
			if (header) Gui_DrawCover(header->id);
			redraw_cover = 0;
		}
		
		u32 buttons = Wpad_WaitButtonsRpt();
		menu_move(&menu, buttons);

		int change = 0;
		if (buttons & WPAD_BUTTON_LEFT || buttons & WPAD_BUTTON_RIGHT) change = -1;
		if (buttons & WPAD_BUTTON_A || buttons & WPAD_BUTTON_2) change = +1;

		if (change) {
			for (n=0; n<sortCnt; n++) {
				if (menu.current == n) {
					if (change == -1) descend[n] = !descend[n];
					else {
						if (descend[n]) {
							sort_desc = 1;
							sortList(sortTypes[n].sortDsc);
						} else {
							sort_desc = 0;
							sortList(sortTypes[n].sortAsc);
						}
						redraw_cover = 1;
						sort_index = n;
						break;
					}
				}
			}
		}
		
		// HOME button
		if (buttons & WPAD_BUTTON_HOME) {
			Handle_Home(0);
		}
		if (buttons & WPAD_BUTTON_B) break;
	}
	return 0;
}
