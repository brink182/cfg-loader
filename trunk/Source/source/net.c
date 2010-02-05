
// by oggzee

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <unistd.h>
#include <sys/stat.h>

#include "disc.h"
#include "gui.h"
#include "wpad.h"
#include "cfg.h"
#include "http.h"
#include "dns.h"
#include "video.h"
#include "net.h"

extern struct discHdr *gameList;
extern s32 gameCnt, gameSelected, gameStart;
extern bool imageNotFound;

static int net_top = -1;

/*Networking - Forsaekn*/
int Net_Init(char *ip){
	
	s32 result;

	int count = 0;
	while ((result = net_init()) == -EAGAIN && count < 100)	{
		printf(".");
		fflush(stdout);
		usleep(50 * 1000); //50ms
		count++;
	}

	if (result >= 0)
	{
		char myIP[16];

		net_top = result;
		if (if_config(myIP, NULL, NULL, true) < 0)
		{
			printf("Error reading IP address.");
			return false;
		}
		else
		{
			return true;
		}
	}
	return false;
}

#ifdef DEBUG_NET
#define debug_printf(fmt, args...) \
	fprintf(stderr, "%s:%d:" fmt, __FUNCTION__, __LINE__, ##args)
#else
#define debug_printf(fmt, args...)
#endif // DEBUG_NET

#define STACK_ALIGN(type, name, cnt, alignment)         u8 _al__##name[((sizeof(type)*(cnt)) + (alignment) + (((sizeof(type)*(cnt))%(alignment)) > 0 ? ((alignment) - ((sizeof(type)*(cnt))%(alignment))) : 0))]; \
	type *name = (type*)(((u32)(_al__##name)) + ((alignment) - (((u32)(_al__##name))&((alignment)-1))))

#define IOCTL_NWC24_CLEANUP 0x07

#define NET_UNKNOWN_ERROR_OFFSET	-10000

static char __kd_fs[] ATTRIBUTE_ALIGN(32) = "/dev/net/kd/request";

// 0 means we don't know what this error code means
// I sense a pattern here...
static u8 _net_error_code_map[] = {
	0, // 0
	0, 
	0, 
	0,
	0, 
	0, // 5
	EAGAIN, 
	EALREADY,
	EBADFD,
	0,
	0, // 10
	0,
	0,
	0,
	0,
	0, // 15
	0,
	0,
	0,
	0,
	0, // 20
	0,
	0,
	0,
	0,
	0, // 25
	EINPROGRESS,
	0,
	0,
	0,
	EISCONN, // 30
	0,
	0,
	0,
	0,
	0, // 35
	0,
	0,
	0,
	ENETDOWN, //?
	0, // 40
	0,
	0,
	0,
	0,
	0, // 45
	0,
	0,
	0,
	0,
	0, // 50
	0,
	0,
	0,
	0,
	0, // 55
	0,
	0,
	0,
	0,
	0, // 60
};

static s32 _net_convert_error(s32 ios_retval)
{
//	return ios_retval;
	if (ios_retval >= 0) return ios_retval;
	if (ios_retval < -sizeof(_net_error_code_map)
		|| !_net_error_code_map[-ios_retval])
			return NET_UNKNOWN_ERROR_OFFSET + ios_retval;
	return -_net_error_code_map[-ios_retval];
}

// by oggzee
static s32 NWC24iCleanupSocket(void)
{
	s32 kd_fd, ret;
	STACK_ALIGN(u8, kd_buf, 0x20, 32);
	
	kd_fd = _net_convert_error(IOS_Open(__kd_fs, 0));
	if (kd_fd < 0) {
		debug_printf("IOS_Open(%s) failed with code %d\n", __kd_fs, kd_fd);
		return kd_fd;
	}
  
	ret = _net_convert_error(IOS_Ioctl(kd_fd, IOCTL_NWC24_CLEANUP, NULL, 0, kd_buf, 0x20));
	if (ret < 0) debug_printf("IOS_Ioctl(7)=%d\n", ret);
  	IOS_Close(kd_fd);
  	return ret;
}

void Net_Close(int close_wc24)
{
	if (net_top >= 0) {
		if (close_wc24) {
			NWC24iCleanupSocket();
			IOS_Close(net_top);
		}
		net_top = -1;
	}
}
/*

Channel regions:

A 	41 	All regions. System channels like the Mii channel use it.
D 	44 	German-speaking regions. Only if separate versions exist, e.g. Zelda: A Link to the Past
E 	45 	USA and other NTSC regions except Japan
F 	46 	French-speaking regions. Only if separate versions exist, e.g. Zelda: A Link to the Past.
J 	4A 	Japan
K 	4B 	Korea
L 	4C 	Japanese Import to Europe, Australia and other PAL regions
M 	4D 	American Import to Europe, Australia and other PAL regions
N 	4E 	Japanese Import to USA and other NTSC regions
P 	50 	Europe, Australia and other PAL regions
Q 	51 	Korea with Japanese language.
T 	54 	Korea with English language.
X 	58 	Not a real region code. Used by DVDX and Homebrew Channel. 

Game regions:

H	Netherlands (quite rare)
I	Italy
S	Spain
X	PAL (not sure if it is for a specific country,
	but it was confirmed to be used on disc that only had French)
Y	PAL, probably the same as for X

*/   

char *gameid_to_cc(char *gameid)
{
	// language codes:
	//char *EN = "EN";
	char *US = "US";
	char *FR = "FR";
	char *DE = "DE";
	char *JA = "JA";
	char *KO = "KO";
	char *NL = "NL";
	char *IT = "IT";
	char *ES = "ES";
	char *ZH = "ZH";
	char *PAL = "PAL";
	// gameid regions:
	switch (gameid[3]) {
		// channel regions:
		case 'A': return PAL;// All regions. System channels like the Mii channel use it.
		case 'D': return DE; // German-speaking regions.
		case 'E': return US; // USA and other NTSC regions except Japan
		case 'F': return FR; // French-speaking regions.
		case 'J': return JA; // Japan
		case 'K': return KO; // Korea
		case 'L': return PAL;// Japanese Import to Europe, Australia and other PAL regions
		case 'M': return PAL;// American Import to Europe, Australia and other PAL regions
		case 'N': return JA; // Japanese Import to USA and other NTSC regions
		case 'P': return PAL;// Europe, Australia and other PAL regions
		case 'Q': return KO; // Korea with Japanese language.
		case 'T': return KO; // Korea with English language.
		// game regions:
		case 'H': return NL; // Netherlands
		case 'I': return IT; // Italy
		case 'S': return ES; // Spain
		case 'X': return PAL;
		case 'Y': return PAL;
		case 'W': return ZH; // Taiwan
	}
	return PAL;
}

char *lang_to_cc()
{
	//printf("lang: %d\n", CONF_GetLanguage());
	switch(CONF_GetLanguage()){
		case CONF_LANG_JAPANESE: return "JA";
		case CONF_LANG_ENGLISH:  return "EN";
		case CONF_LANG_GERMAN:   return "DE";
		case CONF_LANG_FRENCH:   return "FR";
		case CONF_LANG_SPANISH:  return "ES";
		case CONF_LANG_ITALIAN:  return "IT";
		case CONF_LANG_DUTCH:    return "NL";
		case CONF_LANG_KOREAN:   return "KO";
		case CONF_LANG_SIMP_CHINESE: return "EN";
		case CONF_LANG_TRAD_CHINESE: return "EN";
	}
	return "EN";
}

char *auto_cc()
{
	//printf("area: %d\n", CONF_GetArea());
	switch(CONF_GetArea()){
		case CONF_AREA_AUS: return "AU";
		case CONF_AREA_BRA: return "PT";
	}
	return lang_to_cc();
}

char *get_cc()
{
	if (*CFG.download_cc_pal) {
		return CFG.download_cc_pal;
	} else {
		return auto_cc();
	}
}

static bool fmt_cc_pal = false;

void format_URL(char *game_id, char *url, int size)
{
	// widescreen
	int width, height;
	/*if (CFG.widescreen && CFG.download_wide) {
		width = CFG.W_COVER_WIDTH;
		height = CFG.W_COVER_HEIGHT;
	} else {*/
		width = CFG.N_COVER_WIDTH;
		height = CFG.N_COVER_HEIGHT;
	//}
	// region
	char region[10];
	switch(game_id[3]){
	case 'E':
		sprintf(region,"ntsc");
		break;
	case 'J':
		sprintf(region,"ntscj");
		break;
	default:
	case 'P':
		sprintf(region,"pal");
		break;
	}
	// country code
	fmt_cc_pal = false;
	char *cc = gameid_to_cc(game_id);
	if (strcmp(cc, "PAL") == 0) {
		cc = get_cc();
		if (strcmp(cc, "EN") != 0) {
			fmt_cc_pal = true;
		}
	}
	// set URL
	char id[8];
	char tmps[15];
	STRCOPY(id, game_id);
	str_replace(url, "{REGION}", region, size);
	if (str_replace(url, "{CC}", cc, size) == false) fmt_cc_pal = false;
	id[6] = 0;
	str_replace(url, "{ID6}", id, size);
	id[4] = 0;
	str_replace(url, "{ID4}", id, size);
	id[3] = 0;
	str_replace(url, "{ID3}", id, size);
	sprintf(tmps, "%d", width);
	str_replace(url, "{WIDTH}", tmps, size);
	sprintf(tmps, "%d", height);
	str_replace(url, "{HEIGHT}", tmps, size);

	dbg_printf("URL: %s\n", url);
}

struct block Download_URL(char *id, char *url_list)
{
	struct block file;
	char *next_url;
	char url_fmt[200];
	char url[200];
	int retry_cnt = 0;
	int id_cnt;
	int pal_cnt;

	next_url = url_list;

	retry_url:

	next_url = split_token(url_fmt, next_url, ' ', sizeof(url_fmt));

	pal_cnt = 0;

	retry_cc_pal:

	id_cnt = 3;

	retry_id:

	if (retry_cnt > 0) printf("    Trying (url#%d) ...\n", retry_cnt+1);

	STRCOPY(url, url_fmt);

	if (pal_cnt) {
		str_replace(url, "{CC}", "EN", sizeof(url));
	}

	if (strstr(url, "{ID}")) {
		// {ID} auto retries with 6,4,3
		switch (id_cnt) {
		case 3:
			str_replace(url, "{ID}", "{ID6}", sizeof(url));
			break;
		case 2:
			str_replace(url, "{ID}", "{ID4}", sizeof(url));
			break;
		case 1:
			str_replace(url, "{ID}", "{ID3}", sizeof(url));
			break;
		}
		id_cnt--;
	} else {
		id_cnt = 0;
	}

	format_URL(id, url, sizeof(url));

	if (url[0] == 0) {
		printf("    Error: no URL.\n");
		goto dl_err;
	}

	__console_flush(0);
	
	file = downloadfile(url);
	
	if (file.data == NULL || file.size == 0) {
		printf("%s\n", url);
		printf("    Error: no data.\n");
		goto dl_err;
	}
	printf("    Size: %d byte", file.size);

	// verify if valid png
	s32 ret;
	u32 w, h;
	ret = __Gui_GetPngDimensions(file.data, &w, &h);
	if (ret) {
		printf("\n%s\n", url);
		printf("    Error: Invalid PNG image!\n");
		SAFE_FREE(file.data);
		goto dl_err;
	}

	// success
	goto out;

	// error
	dl_err:
	retry_cnt++;
	if (id_cnt > 0) goto retry_id;
	if (fmt_cc_pal && pal_cnt == 0) {
		pal_cnt++;
		goto retry_cc_pal;
	}
	if (next_url) goto retry_url;
	file.data = NULL;
	file.size = 0;
	out:
	if (retry_cnt > 0) {
		//sleep(2);
		if (CFG.debug) { printf("Press any button.\n"); Wpad_WaitButtons(); }
	}
		
	return file;
}

char *get_style_name(int style)
{
	switch (style) {
		default:
		case CFG_COVER_STYLE_2D: return "FLAT";
		case CFG_COVER_STYLE_3D: return "3D";
		case CFG_COVER_STYLE_DISC: return "DISC";
		case CFG_COVER_STYLE_FULL: return "FULL";
	}
}

void Download_Cover_Style(char *id, int style)
{
	char imgName[100];
	char imgPath[100];
	char *style_name = "";
	char *path = "";
	char *url = "";
	char *wide = "";
	//int do_wide = 0;

	/*if (CFG.widescreen && CFG.download_wide) {
		wide = "_wide";
		do_wide = 1;
	}*/
	style_name = get_style_name(style);
	switch (style) {
		default:
		case CFG_COVER_STYLE_2D:
			//if (do_wide) url = CFG.cover_url_2d_wide;
			//else
			url = CFG.cover_url_2d_norm;
			path = CFG.covers_path_2d;
			break;

		case CFG_COVER_STYLE_3D:
			//if (do_wide) url = CFG.cover_url_3d_wide;
			//else
			url = CFG.cover_url_3d_norm;
			path = CFG.covers_path_3d;
			break;

		case CFG_COVER_STYLE_DISC:
			//if (do_wide) url = CFG.cover_url_disc_wide;
			//else
			url = CFG.cover_url_disc_norm;
			path = CFG.covers_path_disc;
			break;

		case CFG_COVER_STYLE_FULL:
			url = CFG.cover_url_full_norm;
			path = CFG.covers_path_full;
			break;
	}

	if (CFG.download_id_len == 6) {
		// save as 6 character ID
		snprintf(imgName, sizeof(imgName), "%.6s%s.png", id, wide);
	} else {
		// save as 4 character ID
		snprintf(imgName, sizeof(imgName), "%.4s%s.png", id, wide);
	}
	printf("    Downloading %s cover... (%s)\n", style_name, imgName);
	snprintf(imgPath, sizeof(imgPath), "%s/%s", path, imgName);

	// try to download image
	struct block file;
	file = Download_URL(id, url);
	if (file.data == NULL) {
		goto dl_err;
	}

	// check path access
	struct stat st;
	char drive_root[8];
	snprintf(drive_root, sizeof(drive_root), "%s/", FAT_DRIVE);
	if (stat(drive_root, &st)) {
		printf("    ERROR: %s is not accessible\n", drive_root);
		goto dl_err;
	}
	if (stat(path, &st)) {
		if (mkdir(path, 0777)) {
			printf("    Cannot create dir: %s\n", path);
			goto dl_err;
		}
	}
		
	// save png to sd card for future use
	FILE *f;
	f = fopen(imgPath, "wb");
	if (!f) {
		printf("\n    Error opening: %s\n", imgPath);
		goto dl_err;
	}
	int ret = fwrite(file.data,file.size,1,f);
	fclose (f);
	free(file.data);
	if (ret != 1) {
		printf(" ERROR: writing %s (%d).\n", imgPath, ret);
		remove(imgPath);
	} else {
		printf(" Download complete.\n");
	}
	__console_flush(0);

	//refresh = true;				
	//sleep(3);
	return;

	dl_err:
	__console_flush(0);
	sleep(2);
	if (CFG.debug) { printf("Press any button.\n"); Wpad_WaitButtons(); }
}

/* Initialize Network */
bool Init_Net()
{
	static bool firstTimeDownload = true;

	if(firstTimeDownload == true){
		char myIP[16];
		printf("[+] Initializing Network...\n");
		if( !Net_Init(myIP) ){
			printf("    Error Initializing Network.\n");
			return false;
		}
		printf("    Network connection established.\n");
		firstTimeDownload = false;
	}
	return true;
}

void Download_Cover_Missing(char *id, int style, bool missing_only, bool verbose)
{
	int ret;
	void *data = NULL;

	if (!id) return;

	if (missing_only) {
		// check if missing
		ret = Gui_LoadCover_style((u8*)id, &data, false, style);
		if (ret > 0 && data) {
			// found. verify if valid png
			u32 width, height;
			ret = __Gui_GetPngDimensions(data, &width, &height);
			SAFE_FREE(data);
			if (ret == 0 && width > 0 && height > 0) {
				// all ok.
				if (verbose) {
					printf("    Found %s Cover\n", get_style_name(style));
				}
				return;
			}
		}
	}

	Download_Cover_Style(id, style);
}

void Download_Cover(char *id, bool missing_only, bool verbose)
{
	if (!id)
		return;

	if (strlen(id) < 4) {
		printf("ERROR: Invalid Game ID\n");
		goto dl_err;
	}

	if (verbose) {
		printf("[+] Downloading %s Covers for %.4s\n\n",
			missing_only ? "Missing" : "", id);
	}

	//the first time no image is found, attempt to init network
	if(!Init_Net()) goto dl_err;

	if (CFG.download_all) {
		Download_Cover_Missing(id, CFG_COVER_STYLE_2D, missing_only, verbose);
		Download_Cover_Missing(id, CFG_COVER_STYLE_3D, missing_only, verbose);
		Download_Cover_Missing(id, CFG_COVER_STYLE_DISC, missing_only, verbose);
		Download_Cover_Missing(id, CFG_COVER_STYLE_FULL, missing_only, verbose);
	} else {
		Download_Cover_Missing(id, CFG.cover_style, missing_only, verbose);
	}

	return;
	dl_err:
	sleep(3);
}

void Download_All_Covers(bool missing_only)
{
	int i;
	struct discHdr *header;
	u32 buttons;
	printf("\n[+] Downloading All %sCovers\n\n", missing_only?"Missing ":"");
	printf("    Hold button B to cancel.\n\n");
	//dbg_time1();
	for (i=0; i<gameCnt; i++) {
		buttons = Wpad_GetButtons();
		if (buttons & WPAD_BUTTON_B) {
			printf("\n    Cancelled.\n");
			sleep(1);
			return;
		}
		header = &gameList[i];
		printf("%d / %d %.4s %.25s\n", i, gameCnt, header->id, get_title(header));
		Gui_DrawCover(header->id);
		Download_Cover((char*)header->id, missing_only, false);
		Gui_DrawCover(header->id);
	}
	printf("\n    Done.\n");
	//unsigned ms = dbg_time2(NULL);
	//printf("    Time: %.2f seconds\n", (float)ms/1000); Wpad_WaitButtons();
	sleep(1);
}


