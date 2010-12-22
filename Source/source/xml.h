#ifndef _XML_H_
#define _XML_H_

#ifdef __cplusplus
extern "C"
{
#endif

struct gameXMLinfo
{	
	char id[7];
	char region[7];
	char title[100];
	char synopsis[500];
	char developer[40];
	char publisher[40];
	int year;
	int month;
	int day;
	char genre[75];
	char ratingtype[5];
	char ratingvalue[5];
	//char ratingdescriptors[16][40];
	int wifiplayers;
	char wififeatures[8][15];
	int players;
	char accessories[16][20];
	char accessoriesReq[16][20];
	u32 caseColor;
};

struct gameXMLinfo *game_info;
char * getLang(int lang);
bool ReloadXMLDatabase(char* xmlfilepath, char* argdblang, bool argJPtoEN);
void CloseXMLDatabase();
bool LoadGameInfoFromXML(u8 * gameid);
char *ConvertLangTextToCode(char *langtext);
char *VerifyLangCode(char *langtext);
void ConvertRating(char *ratingvalue, char *fromrating, char *torating, char *destvalue, int destsize);
void PrintGameInfo(bool showfullinfo);
int getIndexFromId(u8 * gameid);

bool DatabaseLoaded(void);
int getControllerTypes(char *controller, u8 * gameid);
bool hasGenre(char *genre, u8 * gameid);
bool hasFeature(char *feature, u8 *gameid);
bool xml_getCaseColor(u32 *color, u8 *gameid);

#ifdef __cplusplus
}
#endif

#endif
