/*
Load game information from XML - Lustar
*/
#include <zlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "util.h"
#include "cfg.h"
#include "xml.h"
#include "wbfs.h"
#include "menu.h"
#include "mem.h"
#include "wpad.h"
#include "gettext.h"

#include "unzip/unzip.h"
#include "unzip/miniunz.h"

/* config */
char xmlCfgLang[3];
char xmlcfg_filename[100];
char * xmlData;
static struct gameXMLinfo gameinfo;
static struct gameXMLinfo gameinfo_reset;
int xmlgameCnt;

bool xml_loaded = false;
int array_size = 0;

extern int all_gameCnt;
extern long long gettime();
extern u32 diff_msec(long long start,long long end);
bool db_debug = 0;

static char langlist[11][22] =
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

static char langcodes[11][3] =
{{""},
{"JA"},
{"EN"},
{"DE"},
{"FR"},
{"ES"},
{"IT"},
{"NL"},
{"ZH"},
{"ZH"},
{"KO"}};


void xml_stat()
{
	printf("  Games in XML: %d (%d) / %d\n", xmlgameCnt, array_size, all_gameCnt);
}

char * getLang(int lang) {
	if (lang < 1 || lang > 10) return "EN";
	return langcodes[lang+1];
}

char * unescape(char *input, int size) {
	str_replace_all(input, "&gt;", ">", size);
	str_replace_all(input, "&lt;", "<", size);
	str_replace_all(input, "&quot;", "\"", size);
	str_replace_all(input, "&apos;", "'", size);
	str_replace_all(input, "&amp;", "&", size);
	return input;
}

void wordWrap(char *tmp, char *input, int width, int maxLines, int length)
{
	int maxLength = width * maxLines;
	char *whitespace = NULL;
	unescape(input, length);
	char dots[4] = {0xE2, 0x80, 0xA6, 0};
	str_replace_all(input, dots, "...", length);
	strncpy(tmp, input, maxLength);
	int lineLength = 0;
	int wordLength = 0;
	int lines = 1;
	
	char * ptr=tmp;
	while (*ptr!='\0') {
		if (*ptr == '\n' || *ptr == '\r')
			*ptr = ' ';
		ptr++;
	}
	str_replace_all(tmp, "  ", " ", sizeof(tmp));
	
	for (ptr=tmp;*ptr!=0;ptr++) {
		if (lines >= maxLines) *ptr = 0;
		if (*ptr == ' ') {
			if (lineLength + wordLength > width) {
				if (whitespace) {
					*whitespace = '\n';
					lineLength = ptr - whitespace;
					whitespace = NULL;
				} else {
					*ptr = '\n';
					lineLength = 0;
				}
				lines++;
			} else {
				whitespace = ptr;
				lineLength += wordLength+1;
			}
			wordLength = 0;
		} else if (*ptr == '\n') {
			lineLength = 0;
			wordLength = 0;
		} else {
			wordLength++;
		}
	}
	if (length > maxLength && lineLength <= (width - 3)) { 
		strncat(tmp, "...", 3);
	}
}

//1 = Required, 0 = Usable, -1 = Not Used
//Controller = wiimote, nunchuk, classic, guitar, etc
int getControllerTypes(char *controller, u8 * gameid)
{
	int id = getIndexFromId(gameid);
	if (id < 0)
		return -1;
	int i = 0;
	for (;i<16;i++) {
		if (!strnicmp(game_info[id].accessoriesReq[i], controller, 20)) {
			return 1;
		} else if (!strnicmp(game_info[id].accessories[i], controller, 20)) {
			return 0;
		}
	}
	return -1;
}

bool hasFeature(char *feature, u8 *gameid)
{
	int id = getIndexFromId(gameid);
	if (id < 0)
		return 0;
	int i = 0;
	for (;i<8;i++) {
		if (!strnicmp(game_info[id].wififeatures[i], feature, 15)) {
			return 1;
		}
	}
	return 0;
}

bool hasGenre(char *genre, u8 * gameid)
{
	int id = getIndexFromId(gameid);
	if (id < 0)
		return 0;
	if (strstr(game_info[id].genre, genre)) {
		return 1;
	}
	return 0;
}

bool DatabaseLoaded() {
	return xml_loaded;
}

void CloseXMLDatabase()
{
	//Doesn't work?
    SAFE_FREE(game_info);
	xml_loaded = false;
	array_size = 0;
}

bool OpenXMLFile(char *filename)
{
	gameinfo = gameinfo_reset;
	xml_loaded = false;
	char* strresult = strstr(filename,".zip");
    if (strresult == NULL) {
		FILE *filexml;
		filexml = fopen(filename, "rb");
		if (!filexml)
			return false;
		
		fseek(filexml, 0 , SEEK_END);
		u32 size = ftell(filexml);
		rewind(filexml);
		xmlData = mem_alloc(size);
		memset(xmlData, 0, size);
		if (xmlData == NULL) {
			fclose(filexml);
			return false;
		}
		u32 ret = fread(xmlData, 1, size, filexml);
		fclose(filexml);
		if (ret != size)
			return false;
	} else {
		unzFile unzfile = unzOpen(filename);
		if (unzfile == NULL)
			return false;
			
		unzOpenCurrentFile(unzfile);
		unz_file_info zipfileinfo;
		unzGetCurrentFileInfo(unzfile, &zipfileinfo, NULL, 0, NULL, 0, NULL, 0);	
		int zipfilebuffersize = zipfileinfo.uncompressed_size;
		if (db_debug) {
			printf("uncompressed xml: %dkb\n", zipfilebuffersize/1024);
		}
		xmlData = mem_alloc(zipfilebuffersize);
		memset(xmlData, 0, zipfilebuffersize);
		if (xmlData == NULL) {
			unzCloseCurrentFile(unzfile);
			unzClose(unzfile);
			return false;
		}
		unzReadCurrentFile(unzfile, xmlData, zipfilebuffersize);
		unzCloseCurrentFile(unzfile);
		unzClose(unzfile);
	}

	xml_loaded = (xmlData == NULL) ? false : true;
	return xml_loaded;
}

/* convert language text into ISO 639 two-letter language code */
char *ConvertLangTextToCode(char *languagetxt)
{
	if (!strcmp(languagetxt, ""))
		return "EN";
	int i;
	for (i=1;i<=10;i++)
	{
		if (!strcasecmp(languagetxt,langlist[i])) // case insensitive comparison
			return langcodes[i];
	}
	return "EN";
}

char *VerifyLangCode(char *languagetxt)
{
	if (!strcmp(languagetxt, ""))
		return "EN";
	int i;
	for (i=1;i<=10;i++)
	{
		if (!strcasecmp(languagetxt,langcodes[i])) // case insensitive comparison
			return langcodes[i];
	}
	return "EN";
}

char ConvertRatingToIndex(char *ratingtext)
{
	int type = -1;
	if (!strcmp(ratingtext,"CERO")) { type = 0; }
	if (!strcmp(ratingtext,"ESRB")) { type = 1; }
	if (!strcmp(ratingtext,"PEGI")) { type = 2; }
	return type;
}

void ConvertRating(char *ratingvalue, char *fromrating, char *torating, char *destvalue, int destsize)
{
	if(!strcmp(fromrating,torating)) {
		strlcpy(destvalue,ratingvalue,destsize);
		return;
	}

	strcpy(destvalue,"");
	int type = -1;
	int desttype = -1;

	type = ConvertRatingToIndex(fromrating);
	desttype = ConvertRatingToIndex(torating);
	if (type == -1 || desttype == -1)
		return;
	
	/* rating conversion table */
	/* the list is ordered to pick the most likely value first: */
	/* EC and AO are less likely to be used so they are moved down to only be picked up when converting ESRB to PEGI or CERO */
	/* the conversion can never be perfect because ratings can differ between regions for the same game */
	char ratingtable[12][3][4] =
	{
		{{"A"},{"E"},{"3"}},
		{{"A"},{"E"},{"4"}},
		{{"A"},{"E"},{"6"}},
		{{"A"},{"E"},{"7"}},
		{{"A"},{"EC"},{"3"}},
		{{"A"},{"E10+"},{"7"}},
		{{"B"},{"T"},{"12"}},
		{{"D"},{"M"},{"18"}},
		{{"D"},{"M"},{"16"}},
		{{"C"},{"T"},{"16"}},
		{{"C"},{"T"},{"15"}},
		{{"Z"},{"AO"},{"18"}},
	};
	
	int i;
	for (i=0;i<=11;i++)
	{
		if (!strcmp(ratingtable[i][type],ratingvalue)) {
			strlcpy(destvalue,ratingtable[i][desttype],destsize);
			return;
		}
	}
}

char * GetLangSettingFromGame(u8 * gameid) {
	int langcode;
	struct Game_CFG game_cfg = CFG_read_active_game_setting((u8*)gameid);
	if (game_cfg.language) {
		langcode = game_cfg.language;
	} else {
		langcode = CFG.game.language;
	}
	char *langtxt = langlist[langcode];
	return langtxt;
}

void strncpySafe(char *out, char *in, int max, int length) {
	strlcpy(out, in, ((max < length+1) ? max : length+1));
}

int intcpy(char *in, int length) {
	char tmp[10];
	strncpy(tmp, in, length);
	return atoi(tmp);
}

void readNode(char * p, char to[], char * open, char * close) {
	char * tmp = strstr(p, open);
	if (tmp == NULL) {
		strcpy(to, "");
	} else {
		char * s = tmp+strlen(open);
		strlcpy(to, s, strstr(tmp, close)-s+1);
	}
}

void readDate(char * tmp, int n) {
	char date[5] = "";
	readNode(tmp, date, "<date year=\"","\" month=\"");
	game_info[n].year = atoi(date);
	strlcpy(date, "", 4);
	readNode(tmp, date, "\" month=\"","\" day=\"");
	game_info[n].month = atoi(date);
	strlcpy(date, "", 4);
	readNode(tmp, date, "\" day=\"","\"/>");
	game_info[n].day = atoi(date);
}

void readRatings(char * start, int n) {
	char *locStart = strstr(start, "<rating type=\"");
	char *locEnd = NULL;
	if (locStart != NULL) {
		locEnd = strstr(locStart, "</rating>");
		if (locEnd == NULL) {
			locEnd = strstr(locStart, "\" value=\"");
			if (locEnd == NULL) return;
			strncpySafe(game_info[n].ratingtype, locStart+14, sizeof(gameinfo.ratingtype), locEnd-(locStart+14));
			locStart = locEnd;
			locEnd = strstr(locStart, "\"/>");
			strncpySafe(game_info[n].ratingvalue, locStart+9, sizeof(gameinfo.ratingvalue), locEnd-(locStart+9));
		} else {
			char * tmp = strndup(locStart, locEnd-locStart);
			char * reset = tmp;
			locEnd = strstr(locStart, "\" value=\"");
			if (locEnd == NULL) return;
			strncpySafe(game_info[n].ratingtype, locStart+14, sizeof(gameinfo.ratingtype), locEnd-(locStart+14));
			locStart = locEnd;
			locEnd = strstr(locStart, "\">");
			strncpySafe(game_info[n].ratingvalue, locStart+9, sizeof(gameinfo.ratingvalue), locEnd-(locStart+9));
			int z = 0;
			while (tmp != NULL) {
				if (z >= 15) break;
				tmp = strstr(tmp, "<descriptor>");
				if (tmp == NULL) {
					break;
				} else {
					char * s = tmp+strlen("<descriptor>");
					tmp = strstr(tmp+1, "</descriptor>");
					strncpySafe(game_info[n].ratingdescriptors[z], s, sizeof(game_info[n].ratingdescriptors[z]), tmp-s);
					z++;
				}
			}
			tmp = reset;
			SAFE_FREE(tmp);
		}
		locStart = strstr(locEnd, "<rating type=\"");
	}
}

void readPlayers(char * start, int n) {
	char *locStart = strstr(start, "<input players=\"");
	char *locEnd = NULL;
	if (locStart != NULL) {
		locEnd = strstr(locStart, "</input>");
		if (locEnd == NULL) {
			locEnd = strstr(locStart, "\"/>");
			if (locEnd == NULL) return;
			game_info[n].players = intcpy(locStart+16, locEnd-(locStart+16));
		} else {
			game_info[n].players = intcpy(locStart+16, strstr(locStart, "\">")-(locStart+16));
		}
	}
}

void readWifi(char * start, int n) {
	char *locStart = strstr(start, "<wi-fi players=\"");
	char *locEnd = NULL;
	if (locStart != NULL) {
		locEnd = strstr(locStart, "</wi-fi>");
		if (locEnd == NULL) {
			locEnd = strstr(locStart, "\"/>");
			if (locEnd == NULL) return;
			game_info[n].wifiplayers = intcpy(locStart+16, locEnd-(locStart+16));
		} else {
			game_info[n].wifiplayers = intcpy(locStart+16, strstr(locStart, "\">")-(locStart+16));
			char * tmp = strndup(locStart, locEnd-locStart);
			char * reset = tmp;
			int z = 0;
			while (tmp != NULL) {
				if (z >= 7) break;
				tmp = strstr(tmp, "<feature>");
				if (tmp == NULL) {
					break;
				} else {
					char * s = tmp+strlen("<feature>");
					tmp = strstr(tmp+1, "</feature>");
					strncpySafe(game_info[n].wififeatures[z], s, sizeof(game_info[n].wififeatures[z]), tmp-s);
					z++;
				}
			}
			tmp = reset;
			SAFE_FREE(tmp);
		}
	}
}

void readTitles(char * start, int n) {
	char *locStart;
	char *locEnd = start;
	char tmpLang[3];
	bool foundEn = 0;
	bool foundCfg = 0;
	while ((locStart = strstr(locEnd, "<locale lang=\"")) != NULL) {
		strcpy(tmpLang, "");
		locEnd = strstr(locStart, "</locale>");
		if (locEnd == NULL) {
			locEnd = strstr(locStart, "\"/>");
			if (locEnd == NULL) break;
			continue;
		}
		strlcpy(tmpLang, locStart+14, 3);
		foundCfg = (memcmp(tmpLang, xmlCfgLang, 2) == 0);
		foundEn  = (memcmp(tmpLang, "EN", 2) == 0);
		if (foundCfg || foundEn) {
			char * locTmp = strndup(locStart, locEnd-locStart);
			char * titStart = strstr(locTmp, "<title>");
			if (titStart != NULL) {
				char * titEnd = strstr(titStart, "</title>");
				strncpySafe(game_info[n].title, titStart+7,
						sizeof(game_info[n].title), titEnd-(titStart+7));
				unescape(game_info[n].title, sizeof(game_info[n].title));
			}
			titStart = strstr(locTmp, "<synopsis>");
			if (titStart != NULL) {
				char * titEnd = strstr(titStart, "</synopsis>");
				strncpySafe(game_info[n].synopsis, titStart+10,
						sizeof(game_info[n].synopsis), titEnd-(titStart+10));
			}
			SAFE_FREE(locTmp);
			if (foundCfg) break;
		}
	}
}

void LoadTitlesFromXML(char *langtxt, bool forcejptoen)
{
	int n = 0;
	char * reset = xmlData;
	char * start;
	char * end;
	char * tmp;
	xmlgameCnt = 0;
	bool forcelang = false;
	if (strcmp(langtxt,""))
		forcelang = true;
	if (forcelang)
		strcpy(xmlCfgLang, (strlen(langtxt) == 2) ? VerifyLangCode(langtxt) : ConvertLangTextToCode(langtxt)); // convert language text into ISO 639 two-letter language code
	if (forcejptoen && (!strcmp(xmlCfgLang,"JA")))
		strcpy(xmlCfgLang,"EN");
	while (1) {
		if (xmlgameCnt >= array_size) {
			void *ptr;
			array_size++;
			ptr = mem1_realloc(game_info, (array_size * sizeof(struct gameXMLinfo)));
			if (!ptr) {
				array_size--;
				printf("ERROR: out of memory!\n");
				mem_stat();
				xml_stat();
				sleep(4);
				break;
			}
			game_info = (struct gameXMLinfo*)ptr;
		}
		start = strstr(xmlData, "<game");
		if (start == NULL) {
			break;
		}
		end = strstr(start, "</game");
		if (end == NULL) {
			break;
		}
		tmp = strndup(start, end-start);
		xmlData = end;
		memset(&game_info[n], 0, sizeof(game_info[n]));
		readNode(tmp, game_info[n].id, "<id>", "</id>");//ok
		if (game_info[n].id[0] == '\0') { //WTF? ERROR
			printf(" ID NULL\n");
			SAFE_FREE(tmp);
			break;
		}

		readTitles(tmp, n);
		readRatings(tmp, n);
		readDate(tmp, n);
		readWifi(tmp, n);
		
		int z = 0;
		int y = 0;
		char * p = tmp;
		while (p != NULL) {
			p = strstr(p, "<control type=\"");
			if (p == NULL) {
				break;
			} else {
				if (y >= 15 && z >= 15) break;
				char * s = p+strlen("<control type=\"");
				p = strstr(p+1, "\" required=\"");
				bool required = (*(p+12) == 't' || *(p+12) == 'T');
				if (!required) {
					if (z < 15) strncpySafe(game_info[n].accessories[z], s, sizeof(game_info[n].accessories[z]), p-s);
					z++;
				} else {
					if (y < 15) strncpySafe(game_info[n].accessoriesReq[y], s, sizeof(game_info[n].accessoriesReq[y]), p-s);
					y++;
				}
			}
		}
		
		readNode(tmp, game_info[n].region, "<region>", "</region>");
		readNode(tmp, game_info[n].developer, "<developer>", "</developer>");
		readNode(tmp, game_info[n].publisher, "<publisher>", "</publisher>");
		readNode(tmp, game_info[n].genre, "<genre>", "</genre>");
		readPlayers(tmp, n);
		//ConvertRating(game_info[n].ratingvalue, gameinfo[n].ratingtype, "ESRB");
		SAFE_FREE(tmp);
		n++;
		xmlgameCnt++;
	}
	xmlData = reset;
	SAFE_FREE(xmlData);
	return;
}

/* load renamed titles from proper names and game info XML, needs to be after cfg_load_games */
bool OpenXMLDatabase(char* xmlfilepath, char* argdblang, bool argJPtoEN)
{
	if (xml_loaded) {
		return true;
	}
	long long start = 0;
	if (db_debug) {
		printf("Database Debug Info:\n");
		mem_stat();
		start = gettime();
	}
	bool opensuccess = true;
	sprintf(xmlcfg_filename, "wiitdb.zip");

	char pathname[200];
	snprintf(pathname, sizeof(pathname), "%s", xmlfilepath);
	if (xmlfilepath[strlen(xmlfilepath) - 1] != '/') snprintf(pathname, sizeof(pathname), "%s/",pathname);
	snprintf(pathname, sizeof(pathname), "%s%s", pathname, xmlcfg_filename);
	opensuccess = OpenXMLFile(pathname);
	if (!opensuccess) {
		CloseXMLDatabase();
		sprintf(xmlcfg_filename, "wiitdb_%s.zip", CFG.partition);

		snprintf(pathname, sizeof(pathname), "%s", xmlfilepath);
		if (xmlfilepath[strlen(xmlfilepath) - 1] != '/') snprintf(pathname, sizeof(pathname), "%s/",pathname);
		snprintf(pathname, sizeof(pathname), "%s%s", pathname, xmlcfg_filename);
		opensuccess = OpenXMLFile(pathname);
		if (!opensuccess) {
			CloseXMLDatabase();
			return false;
		}
	}
	LoadTitlesFromXML(argdblang, argJPtoEN);
	if (db_debug) {
		printf_("Load Time: %u ms\n", diff_msec(start, gettime()));
		mem_stat();
		Wpad_WaitButtons();
	}
	return true;
}

bool ReloadXMLDatabase(char* xmlfilepath, char* argdblang, bool argJPtoEN)
{
	if (xml_loaded) {
		if (db_debug) xml_stat();
		CloseXMLDatabase();
	}
	return OpenXMLDatabase(xmlfilepath, argdblang, argJPtoEN);
}

int getIndexFromId(u8 * gameid)
{
	int n = 0;
	if (gameid == NULL)
		return -1;
//	while (1) {
//		if (n >= xmlgameCnt)
//			return -1;
	for (;n<xmlgameCnt;n++) {
		if (!strcmp(game_info[n].id, (char *)gameid)) {
			return n;
		}
//		n++;
	}
	return -1;
}

bool LoadGameInfoFromXML(u8 * gameid)
/* gameid: full game id */
{
	gameinfo = gameinfo_reset;
	int n = getIndexFromId(gameid);
	if (n >= 0) {
		gameinfo = game_info[n];
		return true;
	}
	return false;
}

void RemoveChar(char *string, char toremove)
{
	char *ptr;
	ptr = string;
	while (*ptr!='\0')
	{
		if (*ptr==toremove){
			while (*ptr!='\0'){
				*ptr=*(ptr+1);
				if (*ptr!='\0') ptr++;
			}
			ptr=string;
		}
		ptr++;
	}
}

char *utf8toconsole(char *string)
{
    char *ptr; 
    ptr = string;
	RemoveChar(string,0xC3);
	ptr = string;
	while (*ptr != '\0') {
		switch (*ptr){
			case 0x87: // ﾇ
				*ptr = 0x80; 
				break;
			case 0xA7: // ・
				*ptr = 0x87; 
				break;
			case 0xA0: // ・
				*ptr = 0x85; 
				break;
			case 0xA2: // ・
				*ptr = 0x83; 
				break;
			case 0x80: // ﾀ
				*ptr = 0x41; 
				break;
			case 0x82: // ﾂ
				*ptr = 0x41; 
				break;
			case 0xAA: // ・
				*ptr = 0x88; 
				break;
			case 0xA8: // ・
				*ptr = 0x8A; 
				break;
			case 0xA9: // ・
				*ptr = 0x82;  
				break;	
			case 0x89: // ﾉ
				*ptr = 0x90; 
				break;
			case 0x88: // ﾈ
				*ptr = 0x45; 
				break;
			case 0xC5: // Okami
				*ptr = 0x4F; 
				break;
			case 0xB1: // ・
				*ptr = 0xA4; 
				break;
			case 0x9F: // ﾟ
				*ptr = 0xE1; 
				break;	
				
		}
        ptr++;
    }
	return string;
}

void PrintGameInfo(bool showfullinfo) {

	char linebuf[1000] = "";
	if (gameinfo.year != 0)
		snprintf(linebuf, sizeof(linebuf), "%d ", gameinfo.year);
	if (strcmp(gameinfo.publisher,"") != 0)
		snprintf(linebuf, sizeof(linebuf), "%s%s", linebuf, unescape(gameinfo.publisher, sizeof(gameinfo.publisher)));
	if (strcmp(gameinfo.developer,"") != 0 && strcmp(gameinfo.developer,gameinfo.publisher) != 0)
		snprintf(linebuf, sizeof(linebuf), "%s / %s", linebuf, unescape(gameinfo.developer, sizeof(gameinfo.developer)));
	if (strlen(linebuf) >= 36) {
		char buffer[45] = "";
		strncpy(buffer, linebuf,  36);
		strncat(buffer, "...", 3);
		snprintf(linebuf, sizeof(linebuf), "%s", buffer);
	}
	printf("%s\n",linebuf);
	strcpy(linebuf,"");
		
	if (strcmp(gameinfo.ratingvalue,"") != 0) {
		char rating[8];
		STRCOPY(rating, gameinfo.ratingvalue);
		if (!strcmp(gameinfo.ratingtype,"PEGI")) strcat(rating, "+");
		snprintf(linebuf, sizeof(linebuf), gt("Rated %s"), rating);
		strcat(linebuf, " ");
	}
	if (gameinfo.players != 0) {
		char players[32];
		if (gameinfo.players > 1) {
			snprintf(D_S(players), gt("for %d players"), gameinfo.players);
		} else {
			strcpy(players, gt("for 1 player"));
		}
		strcat(linebuf, players);
		if (gameinfo.wifiplayers > 1) {
			snprintf(D_S(players), gt("(%d online)"), gameinfo.wifiplayers);
			strcat(linebuf, " ");
			strcat(linebuf, players);
		}
	}
	printf("%s",linebuf);
	strcpy(linebuf,"");
	if (showfullinfo) {
		/*
		printf("   ID: %s\n", gameinfo.id);
		printf("   REGION: %s\n", gameinfo.region);
		printf("   TITLE: %s\n", gameinfo.title);
		printf("   DEV: %s\n", gameinfo.developer);
		printf("   PUB: %s\n", gameinfo.publisher);
		printf("   DATE: %d / %d / %d\n", gameinfo.year, gameinfo.month, gameinfo.day);
		printf("   GEN: %s\n", gameinfo.genre);
		printf("   RAT: %s\n", gameinfo.ratingtype);
		printf("   VAL: %s\n", gameinfo.ratingvalue);
		printf("   RAT DESC: %s\n", gameinfo.ratingdescriptors[0]);
		printf("   WIFI: %d\n", gameinfo.wifiplayers);
		printf("   WIFI DESC: %s\n", gameinfo.wififeatures[0]);
		printf("   PLAY: %d\n", gameinfo.players);
		printf("   ACC: %s\n", gameinfo.accessories[0]);
		printf("   REQ ACC: %s\n", gameinfo.accessoriesReq[0]);
		*/
		//INSERT STUFF HERE
		if (db_debug) {
			mem_stat();
			xml_stat();
		}
		wordWrap(linebuf, gameinfo.synopsis, 40, 8, sizeof(gameinfo.synopsis));
		printf("\n\n%s\n",linebuf);
	}
}

// chartowidechar for UTF-8
// adapted from FreeTypeGX.cpp + UTF-8 fix by Rudolph
wchar_t* charToWideChar(char* strChar) {
	wchar_t *strWChar[strlen(strChar) + 1];
	int	bt;
	bt = mbstowcs(*strWChar, strChar, strlen(strChar));
	if(bt > 0) {
		strWChar[bt] = (wchar_t)'\0';
		return *strWChar;
	}
	
	char *tempSrc = strChar;
	wchar_t *tempDest = *strWChar;
	while((*tempDest++ = *tempSrc++));
	
	return *strWChar;
}

