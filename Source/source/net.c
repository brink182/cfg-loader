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

void Net_Close()
{
	if (net_top >= 0) {
		NWC24iCleanupSocket();
		IOS_Close(net_top);
		net_top = -1;
	}
}

void Download_Cover(struct discHdr *header)
{
	static bool firstTimeDownload = true;
	char imgPath[100];

	if (!header)
		return;

	if (strlen((char*)header->id) < 4) {
		printf("ERROR: Invalid Game ID\n");
		goto dl_err;
	}

	//the first time no image is found, attempt to init network
	/* Initialize Network <<<TO BE THREADED>>> */
	if(firstTimeDownload == true){
		char myIP[16];
		printf("[+] Initializing Network...\n");
		if( !Net_Init(myIP) ){
			printf("    Error Initializing Network.\n");
			goto dl_err;
		}
		printf("    Network connection established.\n");
		firstTimeDownload = false;
	}
		
	/*try to download image */
		
	char url[200];

	char region[10];
	switch(header->id[3]){

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
	char *wide = "";
	if (CFG.widescreen) wide = "_wide";
	printf("    Downloading cover... (%.4s%s.png)\n", header->id, wide);
	// save as 4 character ID
	snprintf(imgPath, sizeof(imgPath), "%s/%.4s%s.png", CFG.covers_path, header->id, wide);
	struct stat st;
	char drive_root[8];
	snprintf(drive_root, sizeof(drive_root), "%s/", FAT_DRIVE);
	if (stat(drive_root, &st)) {
		printf("    ERROR: %s is not accessible\n", drive_root);
		goto dl_err;
	}
	if (stat(CFG.covers_path, &st)) {
		if (mkdir(CFG.covers_path, 0777)) {
			printf("    Cannot create dir: %s\n", CFG.covers_path);
			goto dl_err;
		}
	}
		
	/*	printf("\nImgPath: %s", imgPath); */
	if (!CFG.widescreen) {
		strcopy(url, CFG.cover_url_norm, sizeof(url));
	} else {
		strcopy(url, CFG.cover_url_wide, sizeof(url));
	}
	char id[8];
	char tmps[15];
	strcopy(id, (char*)header->id, sizeof(id));
	str_replace(url, "{REGION}", region, sizeof(url));
	id[6] = 0;
	str_replace(url, "{ID6}", id, sizeof(url));
	id[4] = 0;
	str_replace(url, "{ID4}", id, sizeof(url));
	id[3] = 0;
	str_replace(url, "{ID3}", id, sizeof(url));
	sprintf(tmps, "%d", COVER_WIDTH);
	str_replace(url, "{WIDTH}", tmps, sizeof(url));
	sprintf(tmps, "%d", COVER_HEIGHT);
	str_replace(url, "{HEIGHT}", tmps, sizeof(url));

	if (url[0] == 0) {
		printf("    Error: no URL.\n");
		goto dl_err;
	}
			
	struct block file = downloadfile(url);
	
	if(file.data == NULL){
		printf("URL: %s\n", url);
		printf("    Error: no data.\n");
		// retry ntsc?
		goto dl_err;
	}
	printf("    Size: %d byte", file.size);

	// verify if valid png
	s32 ret;
	u32 w, h;
	ret = __Gui_GetPngDimensions(file.data, &w, &h);
	if (ret) {
		printf("\n    Error: Invalid PNG image!\n");
		goto dl_err;
	}

	/* save png to sd card for future use*/
				
	FILE *f;
	f = fopen(imgPath, "wb");
	if (!f) {
		printf("\n    Error opening: %s\n", imgPath);
		goto dl_err;
	}
	fwrite(file.data,1,file.size,f);
	fclose (f);
	free(file.data);
	printf("    Download complete.\n");

	//refresh = true;				
	//sleep(3);
	return;

	dl_err:
	sleep(4);
} /* end download */

void Download_All(bool missing)
{
	int i;
	struct discHdr *header;
	u32 buttons;
	printf("\n[+] Downloading All %sCovers\n\n", missing?"Missing ":"");
	printf("    Press button B to cancel.\n\n");
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
		if (missing && !imageNotFound) continue;
		Download_Cover(header);
		Gui_DrawCover(header->id);
	}
	printf("\n    Done.\n");
	sleep(1);
}


