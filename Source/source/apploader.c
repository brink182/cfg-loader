#include <stdio.h>
#include <ogcsys.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "apploader.h"
#include "wdvd.h"
#include "video.h"
#include "wpad.h"
#include "cfg.h"
#include "patchcode.h" /*FISHEARS*/
//#include "kenobiwii.h" /*FISHEARS*/

/*KENOBI! - FISHEARS*/
extern const unsigned char kenobiwii[];
extern const int kenobiwii_size;

/* Apploader function pointers */
typedef int   (*app_main)(void **dst, int *size, int *offset);
typedef void  (*app_init)(void (*report)(const char *fmt, ...));
typedef void *(*app_final)();
typedef void  (*app_entry)(void (**init)(void (*report)(const char *fmt, ...)), int (**main)(), void *(**final)());

/* Apploader pointers */
static u8 *appldr = (u8 *)0x81200000;


/* Constants */
#define APPLDR_OFFSET	0x2440

/* Variables */
static u32 buffer[0x20] ATTRIBUTE_ALIGN(32);

/* Forward Declarations */
void PatchCountryStrings(void *Address, int Size);
void maindolpatches(void *dst, int len);
bool Load_Dol(void **buffer, int* dollen);
bool Remove_001_Protection(void *Address, int Size);
void Anti_002_fix(void *Address, int Size);
u32 load_dol_image (void *dolstart, bool clear_bss);


static void __noprint(const char *fmt, ...)
{
}

bool compare_videomodes(GXRModeObj* mode1, GXRModeObj* mode2)
{
	if (mode1->viTVMode != mode2->viTVMode || mode1->fbWidth != mode2->fbWidth ||	mode1->efbHeight != mode2->efbHeight || mode1->xfbHeight != mode2->xfbHeight ||
	mode1->viXOrigin != mode2->viXOrigin || mode1->viYOrigin != mode2->viYOrigin || mode1->viWidth != mode2->viWidth || mode1->viHeight != mode2->viHeight ||
	mode1->xfbMode != mode2->xfbMode || mode1->field_rendering != mode2->field_rendering || mode1->aa != mode2->aa || mode1->sample_pattern[0][0] != mode2->sample_pattern[0][0] ||
	mode1->sample_pattern[1][0] != mode2->sample_pattern[1][0] ||	mode1->sample_pattern[2][0] != mode2->sample_pattern[2][0] ||
	mode1->sample_pattern[3][0] != mode2->sample_pattern[3][0] ||	mode1->sample_pattern[4][0] != mode2->sample_pattern[4][0] ||
	mode1->sample_pattern[5][0] != mode2->sample_pattern[5][0] ||	mode1->sample_pattern[6][0] != mode2->sample_pattern[6][0] ||
	mode1->sample_pattern[7][0] != mode2->sample_pattern[7][0] ||	mode1->sample_pattern[8][0] != mode2->sample_pattern[8][0] ||
	mode1->sample_pattern[9][0] != mode2->sample_pattern[9][0] ||	mode1->sample_pattern[10][0] != mode2->sample_pattern[10][0] ||
	mode1->sample_pattern[11][0] != mode2->sample_pattern[11][0] || mode1->sample_pattern[0][1] != mode2->sample_pattern[0][1] ||
	mode1->sample_pattern[1][1] != mode2->sample_pattern[1][1] ||	mode1->sample_pattern[2][1] != mode2->sample_pattern[2][1] ||
	mode1->sample_pattern[3][1] != mode2->sample_pattern[3][1] ||	mode1->sample_pattern[4][1] != mode2->sample_pattern[4][1] ||
	mode1->sample_pattern[5][1] != mode2->sample_pattern[5][1] ||	mode1->sample_pattern[6][1] != mode2->sample_pattern[6][1] ||
	mode1->sample_pattern[7][1] != mode2->sample_pattern[7][1] ||	mode1->sample_pattern[8][1] != mode2->sample_pattern[8][1] ||
	mode1->sample_pattern[9][1] != mode2->sample_pattern[9][1] ||	mode1->sample_pattern[10][1] != mode2->sample_pattern[10][1] ||
	mode1->sample_pattern[11][1] != mode2->sample_pattern[11][1] || mode1->vfilter[0] != mode2->vfilter[0] ||
	mode1->vfilter[1] != mode2->vfilter[1] ||	mode1->vfilter[2] != mode2->vfilter[2] || mode1->vfilter[3] != mode2->vfilter[3] ||	mode1->vfilter[4] != mode2->vfilter[4] ||
	mode1->vfilter[5] != mode2->vfilter[5] ||	mode1->vfilter[6] != mode2->vfilter[6] )
	{
		return false;
	} else
	{
		return true;
	}
}


void patch_videomode(GXRModeObj* mode1, GXRModeObj* mode2)
{
	mode1->viTVMode = mode2->viTVMode;
	mode1->fbWidth = mode2->fbWidth;
	mode1->efbHeight = mode2->efbHeight;
	mode1->xfbHeight = mode2->xfbHeight;
	mode1->viXOrigin = mode2->viXOrigin;
	mode1->viYOrigin = mode2->viYOrigin;
	mode1->viWidth = mode2->viWidth; 
	mode1->viHeight = mode2->viHeight;
	mode1->xfbMode = mode2->xfbMode; 
	mode1->field_rendering = mode2->field_rendering;
	mode1->aa = mode2->aa;
	mode1->sample_pattern[0][0] = mode2->sample_pattern[0][0];
	mode1->sample_pattern[1][0] = mode2->sample_pattern[1][0];
	mode1->sample_pattern[2][0] = mode2->sample_pattern[2][0];
	mode1->sample_pattern[3][0] = mode2->sample_pattern[3][0];
	mode1->sample_pattern[4][0] = mode2->sample_pattern[4][0];
	mode1->sample_pattern[5][0] = mode2->sample_pattern[5][0];
	mode1->sample_pattern[6][0] = mode2->sample_pattern[6][0];
	mode1->sample_pattern[7][0] = mode2->sample_pattern[7][0];
	mode1->sample_pattern[8][0] = mode2->sample_pattern[8][0];
	mode1->sample_pattern[9][0] = mode2->sample_pattern[9][0];
	mode1->sample_pattern[10][0] = mode2->sample_pattern[10][0];
	mode1->sample_pattern[11][0] = mode2->sample_pattern[11][0];
	mode1->sample_pattern[0][1] = mode2->sample_pattern[0][1];
	mode1->sample_pattern[1][1] = mode2->sample_pattern[1][1];	
	mode1->sample_pattern[2][1] = mode2->sample_pattern[2][1];
	mode1->sample_pattern[3][1] = mode2->sample_pattern[3][1];	
	mode1->sample_pattern[4][1] = mode2->sample_pattern[4][1];
	mode1->sample_pattern[5][1] = mode2->sample_pattern[5][1];	
	mode1->sample_pattern[6][1] = mode2->sample_pattern[6][1];
	mode1->sample_pattern[7][1] = mode2->sample_pattern[7][1];	
	mode1->sample_pattern[8][1] = mode2->sample_pattern[8][1];
	mode1->sample_pattern[9][1] = mode2->sample_pattern[9][1];	
	mode1->sample_pattern[10][1] = mode2->sample_pattern[10][1];
	mode1->sample_pattern[11][1] = mode2->sample_pattern[11][1]; 
	mode1->vfilter[0] = mode2->vfilter[0];
	mode1->vfilter[1] = mode2->vfilter[1];	
	mode1->vfilter[2] = mode2->vfilter[2];
	mode1->vfilter[3] = mode2->vfilter[3];	
	mode1->vfilter[4] = mode2->vfilter[4];
	mode1->vfilter[5] = mode2->vfilter[5];	
	mode1->vfilter[6] = mode2->vfilter[6];
}

GXRModeObj* vmodes[] = {
	&TVNtsc240Ds,
	&TVNtsc240DsAa,
	&TVNtsc240Int,
	&TVNtsc240IntAa,
	&TVNtsc480IntDf,
	&TVNtsc480IntAa,
	&TVNtsc480Prog,
	&TVMpal480IntDf,
	&TVPal264Ds,
	&TVPal264DsAa,
	&TVPal264Int,
	&TVPal264IntAa,
	&TVPal524IntAa,
	&TVPal528Int,
	&TVPal528IntDf,
	&TVPal574IntDfScale,
	&TVEurgb60Hz240Ds,
	&TVEurgb60Hz240DsAa,
	&TVEurgb60Hz240Int,
	&TVEurgb60Hz240IntAa,
	&TVEurgb60Hz480Int,
	&TVEurgb60Hz480IntDf,
	&TVEurgb60Hz480IntAa,
	&TVEurgb60Hz480Prog,
	&TVEurgb60Hz480ProgSoft,
	&TVEurgb60Hz480ProgAa
};

GXRModeObj* PAL2NTSC[]={
	&TVMpal480IntDf,		&TVNtsc480IntDf,
	&TVPal264Ds,			&TVNtsc240Ds,
	&TVPal264DsAa,			&TVNtsc240DsAa,
	&TVPal264Int,			&TVNtsc240Int,
	&TVPal264IntAa,			&TVNtsc240IntAa,
	&TVPal524IntAa,			&TVNtsc480IntAa,
	&TVPal528Int,			&TVNtsc480IntAa,
	&TVPal528IntDf,			&TVNtsc480IntDf,
	&TVPal574IntDfScale,	&TVNtsc480IntDf,
	&TVEurgb60Hz240Ds,		&TVNtsc240Ds,
	&TVEurgb60Hz240DsAa,	&TVNtsc240DsAa,
	&TVEurgb60Hz240Int,		&TVNtsc240Int,
	&TVEurgb60Hz240IntAa,	&TVNtsc240IntAa,
	&TVEurgb60Hz480Int,		&TVNtsc480IntAa,
	&TVEurgb60Hz480IntDf,	&TVNtsc480IntDf,
	&TVEurgb60Hz480IntAa,	&TVNtsc480IntAa,
	&TVEurgb60Hz480Prog,	&TVNtsc480Prog,
	&TVEurgb60Hz480ProgSoft,&TVNtsc480Prog,
	&TVEurgb60Hz480ProgAa,  &TVNtsc480Prog,
	0,0
};

GXRModeObj* NTSC2PAL[]={
	&TVNtsc240Ds,			&TVPal264Ds,		
	&TVNtsc240DsAa,			&TVPal264DsAa,
	&TVNtsc240Int,			&TVPal264Int,
	&TVNtsc240IntAa,		&TVPal264IntAa,
	&TVNtsc480IntDf,		&TVPal528IntDf,
	&TVNtsc480IntAa,		&TVPal524IntAa,
	&TVNtsc480Prog,			&TVPal528IntDf,
	0,0
};

GXRModeObj* NTSC2PAL60[]={
	&TVNtsc240Ds,			&TVEurgb60Hz240Ds,		
	&TVNtsc240DsAa,			&TVEurgb60Hz240DsAa,
	&TVNtsc240Int,			&TVEurgb60Hz240Int,
	&TVNtsc240IntAa,		&TVEurgb60Hz240IntAa,
	&TVNtsc480IntDf,		&TVEurgb60Hz480IntDf,
	&TVNtsc480IntAa,		&TVEurgb60Hz480IntAa,
	&TVNtsc480Prog,			&TVEurgb60Hz480Prog,
	0,0
};
bool Search_and_patch_Video_Modes(void *Address, u32 Size, GXRModeObj* Table[])
{
	u8 *Addr = (u8 *)Address;
	bool found = 0;
	u32 i;

	while(Size >= sizeof(GXRModeObj))
	{



		for(i = 0; Table[i]; i+=2)
		{


			if(compare_videomodes(Table[i], (GXRModeObj*)Addr))

			{
				found = 1;
				patch_videomode((GXRModeObj*)Addr, Table[i+1]);
				Addr += (sizeof(GXRModeObj)-4);
				Size -= (sizeof(GXRModeObj)-4);
				break;
			}
		}

		Addr += 4;
		Size -= 4;
	}


	return found;
}

s32 Apploader_Run(entry_point *entry)
{
	app_entry appldr_entry;
	app_init  appldr_init;
	app_main  appldr_main;
	app_final appldr_final;

	u32 appldr_len;
	s32 ret;

	/* Read apploader header */
	ret = WDVD_Read(buffer, 0x20, APPLDR_OFFSET);
	if (ret < 0)
		return ret;

	/* Calculate apploader length */
	appldr_len = buffer[5] + buffer[6];

	/* Read apploader code */
	ret = WDVD_Read(appldr, appldr_len, APPLDR_OFFSET + 0x20);
	if (ret < 0)
		return ret;

	/* Set apploader entry function */
	appldr_entry = (app_entry)buffer[4];

	/* Call apploader entry */
	appldr_entry(&appldr_init, &appldr_main, &appldr_final);

	/* Initialize apploader */
	appldr_init(__noprint);

	if (CFG.ios_yal) printf(".");
	for (;;) {
		void *dst = NULL;
		s32   len = 0, offset = 0;

		/* Run apploader main function */
		ret = appldr_main(&dst, &len, &offset);
		if (!ret)
			break;

		/* Read data from DVD */
		WDVD_Read(dst, len, (u64)(offset << 2));
		if (CFG.ios_yal) printf(".");

		maindolpatches(dst, len);

		DCFlushRange(dst, len);
	}

	/* Set entry point from apploader */
	*entry = appldr_final();
	if (CFG.ios_yal) printf("\n\n");

	// alternative dol (WiiPower)
	if (CFG.game.alt_dol)
	{
		void *dolbuffer;
		int dollen;
		printf("[+] Alternative .dol:\n");
		if (Load_Dol(&dolbuffer, &dollen))
		{
			maindolpatches(dolbuffer, dollen);
			Remove_001_Protection(dolbuffer, dollen);
			*entry = (void*)load_dol_image(dolbuffer, true);
			if (*entry == NULL)
			{
				printf("    ERROR: Invalid .dol!\n");
				LARGE_free(dolbuffer);
				return -1;
			}
			printf("    Load OK!\n");
			if (CFG.debug) {
				printf("    Press any button...\n");
				Wpad_WaitButtons();
			}
		} else {
			sleep(2);
		}
		printf("\n");
		__console_flush(0);
		usleep(500000);
	}

	// cios 249 rev13 - 002 fix (by WiiPower)
	*(u32 *)0x80003140 = *(u32 *)0x80003188;
	DCFlushRange((void*)0x80000000, 0x3f00);

	return 0;
}



void maindolpatches(void *dst, int len)
{
	GXRModeObj** table = NULL;

	DCFlushRange(dst, len);
	
	if (CFG.game.video == CFG_VIDEO_PATCH) // patch auto
	{
		switch(CONF_GetVideo())
		{
		case CONF_VIDEO_PAL:
			if(CONF_GetEuRGB60() > 0) 
			{
				table = NTSC2PAL60;
			}	
			else
			{
				table = NTSC2PAL;
			}
			break;

		case CONF_VIDEO_MPAL:
			table = NTSC2PAL;
			break;

		default:
			table = PAL2NTSC;
			break;
		}
		Search_and_patch_Video_Modes(dst, len, table);
	}
	
	// force PAL50 (Narolez)
	if (CFG.game.video == CFG_VIDEO_PAL50) {
		Search_and_patch_Video_Modes(dst, len, NTSC2PAL);
	}
	if (CFG.game.video == CFG_VIDEO_PAL60) {
		Search_and_patch_Video_Modes(dst, len, NTSC2PAL60);
	}
	if (CFG.game.video == CFG_VIDEO_NTSC) {
		Search_and_patch_Video_Modes(dst, len, PAL2NTSC);
	}
	
	if (CFG.game.ocarina) {
		dogamehooks(dst,len);
	}
	if (CFG.game.vidtv) {
		vidolpatcher(dst,len);
	}
	/*LANGUAGE PATCH - FISHEARS*/
	if (CFG.game.language != CFG_LANG_CONSOLE) {
		langpatcher(dst,len);
	}
	// Country Patch by WiiPower
	if(CFG.game.country_patch) {
		PatchCountryStrings(dst, len);
	}

	// 002b fix from NeoGammaR4 by WiiPower:
	if (CFG.game.fix_002) {
	   	Anti_002_fix(dst, len);
	}

	DCFlushRange(dst, len);
}

bool Load_Dol(void **buffer, int* dollen)
{
	int ret;
	FILE* file;
	void* dol_buffer;
	
	char fname[200];
	char gameidbuffer4[5];
	memset(gameidbuffer4, 0, 5);
	memcpy(gameidbuffer4, (char*)0x80000000, 4);		

	snprintf(fname, sizeof(fname), "%s/%s.dol", USBLOADER_PATH, gameidbuffer4);
	printf("    %s\n", fname);

	file = fopen( fname, "rb");
	
	if(file == NULL) 
	{
		printf("    Not Found.\n");
		sleep(1);
		return false;
	}
	
	int filesize;
	fseek(file, 0, SEEK_END);
	filesize = ftell(file);
	fseek(file, 0, SEEK_SET);

	//dol_buffer = malloc(filesize);
	dol_buffer = LARGE_memalign(filesize, 32);
	if (dol_buffer == NULL)
	{
		printf("Out of memory\n");
		return false;
	}
	ret = fread( dol_buffer, 1, filesize, file);
	if(ret != filesize)
	{
		printf("Error reading .dol");
		free(dol_buffer);
		fclose(file);
		return false;
	}
	fclose(file);

	*buffer = dol_buffer;
	*dollen = filesize;
	return true;
}	


static u8  *diskid = (u8  *)0x80000000;

void PatchCountryStrings(void *Address, int Size)
{
    u8 SearchPattern[4]    = { 0x00, 0x00, 0x00, 0x00 };
    u8 PatchData[4]        = { 0x00, 0x00, 0x00, 0x00 };
    u8 *Addr            = (u8*)Address;

    int wiiregion = CONF_GetRegion();

    switch (wiiregion)
    {
        case CONF_REGION_JP:
            SearchPattern[0] = 0x00;
            SearchPattern[1] = 0x4A; // J
            SearchPattern[2] = 0x50; // P
            break;
        case CONF_REGION_EU:
            SearchPattern[0] = 0x02;
            SearchPattern[1] = 0x45; // E
            SearchPattern[2] = 0x55; // U
            break;
        case CONF_REGION_KR:
            SearchPattern[0] = 0x04;
            SearchPattern[1] = 0x4B; // K
            SearchPattern[2] = 0x52; // R
            break;
        case CONF_REGION_CN:
            SearchPattern[0] = 0x05;
            SearchPattern[1] = 0x43; // C
            SearchPattern[2] = 0x4E; // N
            break;
        case CONF_REGION_US:
        default:
            SearchPattern[0] = 0x01;
            SearchPattern[1] = 0x55; // U
            SearchPattern[2] = 0x53; // S
    }

    switch (diskid[3])
    {
        case 'J':
            PatchData[1] = 0x4A; // J
            PatchData[2] = 0x50; // P
            break;

        case 'D':
        case 'F':
        case 'P':
        case 'X':
        case 'Y':
            PatchData[1] = 0x45; // E
            PatchData[2] = 0x55; // U
            break;

        case 'E':
        default:
            PatchData[1] = 0x55; // U
            PatchData[2] = 0x53; // S
    }

    while (Size >= 4)
    {
        if (Addr[0] == SearchPattern[0] && Addr[1] == SearchPattern[1] && Addr[2] == SearchPattern[2] && Addr[3] == SearchPattern[3])
        {
            //*Addr = PatchData[0];
            Addr += 1;
            *Addr = PatchData[1];
            Addr += 1;
            *Addr = PatchData[2];
            Addr += 1;
            //*Addr = PatchData[3];
            Addr += 1;
            Size -= 4;
        } else
        {
            Addr += 4;
            Size -= 4;
        }
    }
}

bool Remove_001_Protection(void *Address, int Size)
{
	u8 SearchPattern[16] = 	{ 0x40, 0x82, 0x00, 0x0C, 0x38, 0x60, 0x00, 0x01, 0x48, 0x00, 0x02, 0x44, 0x38, 0x61, 0x00, 0x18 };
	u8 PatchData[16] = 		{ 0x40, 0x82, 0x00, 0x04, 0x38, 0x60, 0x00, 0x01, 0x48, 0x00, 0x02, 0x44, 0x38, 0x61, 0x00, 0x18 };

	void *Addr = Address;
	void *Addr_end = Address+Size;

	while(Addr <= Addr_end-sizeof(SearchPattern))
	{
		if(memcmp(Addr, SearchPattern, sizeof(SearchPattern))==0) 
		{
			memcpy(Addr,PatchData,sizeof(PatchData));
			return true;
		}
		Addr += 4;
	}
	return false;
}

void Anti_002_fix(void *Address, int Size)
{
	u8 SearchPattern[12] = 	{ 0x2C, 0x00, 0x00, 0x00, 0x48, 0x00, 0x02, 0x14, 0x3C, 0x60, 0x80, 0x00 };
	u8 PatchData[12] = 		{ 0x2C, 0x00, 0x00, 0x00, 0x40, 0x82, 0x02, 0x14, 0x3C, 0x60, 0x80, 0x00 };

	void *Addr = Address;
	void *Addr_end = Address+Size;

	while(Addr <= Addr_end-sizeof(SearchPattern))
	{
		if(memcmp(Addr, SearchPattern, sizeof(SearchPattern))==0) 
		{
			memcpy(Addr,PatchData,sizeof(PatchData));
		}
		Addr += 4;
	}
}

