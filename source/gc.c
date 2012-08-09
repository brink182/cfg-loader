#include <gccore.h>
#include <stdio.h>

#include <sdcard/wiisd_io.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <fat.h>

#include "gc.h"
#include "dir.h"
#include "util.h"
#include "gettext.h"
#include "wpad.h"
#include "debug.h"
#include "disc.h"
#include "fileOps.h"
#include "cfg.h"

#define MAX_FAT_PATH 1024

#define SRAM_ENGLISH 0
#define SRAM_GERMAN 1
#define SRAM_FRENCH 2
#define SRAM_SPANISH 3
#define SRAM_ITALIAN 4
#define SRAM_DUTCH 5

syssram* __SYS_LockSram();
u32 __SYS_UnlockSram(u32 write);
u32 __SYS_SyncSram(void);

extern char wbfs_fs_drive[16];
char dm_boot_drive[16];

void GC_SetVideoMode(u8 videomode)
{
	syssram *sram;
	sram = __SYS_LockSram();
	//void *m_frameBuf;
	static GXRModeObj *rmode;
	int memflag = 0;

	if((VIDEO_HaveComponentCable() && (CONF_GetProgressiveScan() > 0)) && videomode > 3)
		sram->flags |= 0x80; //set progressive flag
	else
		sram->flags &= 0x7F; //clear progressive flag

	if(videomode == 1 || videomode == 3 || videomode == 5)
	{
		memflag = 1;
		sram->flags |= 0x01; // Set bit 0 to set the video mode to PAL
		sram->ntd |= 0x40; //set pal60 flag
	}
	else
	{
		sram->flags &= 0xFE; // Clear bit 0 to set the video mode to NTSC
		sram->ntd &= 0xBF; //clear pal60 flag
	}

	if(videomode == 1)
		rmode = &TVPal528IntDf;
	else if(videomode == 2)
		rmode = &TVNtsc480IntDf;
	else if(videomode == 3)
	{
		rmode = &TVEurgb60Hz480IntDf;
		memflag = 5;
	}
	else if(videomode == 4)
		rmode = &TVNtsc480Prog;
	else if(videomode == 5)
	{
		rmode = &TVEurgb60Hz480Prog;
		memflag = 5;
	}

	__SYS_UnlockSram(1); // 1 -> write changes
	while(!__SYS_SyncSram());

	/* Set video mode to PAL or NTSC */
	*(vu32*)0x800000CC = memflag;
	DCFlushRange((void *)(0x800000CC), 4);
	//ICInvalidateRange((void *)(0x800000CC), 4);

	if (rmode != 0)
		VIDEO_Configure(rmode);
	
	//m_frameBuf = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	//VIDEO_ClearFrameBuffer(rmode, m_frameBuf, COLOR_BLACK);
	//VIDEO_SetNextFramebuffer(m_frameBuf);

 /* Setup video  */
	VIDEO_SetBlack(TRUE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) 
		VIDEO_WaitVSync();
}

u8 get_wii_language()
{
	switch (CONF_GetLanguage())
	{
		case CONF_LANG_GERMAN:
			return SRAM_GERMAN;
		case CONF_LANG_FRENCH:
			return SRAM_FRENCH;
		case CONF_LANG_SPANISH:
			return SRAM_SPANISH;
		case CONF_LANG_ITALIAN:
			return SRAM_ITALIAN;
		case CONF_LANG_DUTCH:
			return SRAM_DUTCH;
		default:
			return SRAM_ENGLISH;
	}
}

void GC_SetLanguage(u8 lang)
{
	if (lang == 0)
		lang = get_wii_language();
	else
		lang--;

	syssram *sram;
	sram = __SYS_LockSram();
	sram->lang = lang;

	__SYS_UnlockSram(1); // 1 -> write changes
	while(!__SYS_SyncSram());
}

s32 DML_RemoveGame(struct discHdr header, bool onlyBootDrive)
{
	char fname[MAX_FAT_PATH];
	
	if (header.magic == GC_GAME_ON_DM_DRIVE)
		snprintf(fname, sizeof(fname), "%s/games/%s/", dm_boot_drive, header.folder);
	else if (header.magic == GC_GAME_ON_GAME_DRIVE && !onlyBootDrive)
		snprintf(fname, sizeof(fname), "%s/games/%s/", wbfs_fs_drive, header.folder);
	else if (header.magic == GC_GAME_ON_SD_DRIVE && !onlyBootDrive)
		snprintf(fname, sizeof(fname), "sd:/games/%s/", header.folder);
	
	fsop_deleteFolder(fname);
	return 0;
}

int DML_GameIsInstalled(char *folder)
{
	int ret = 0;
	char source[300];
	snprintf(source, sizeof(source), "%s/games/%s/game.iso", dm_boot_drive, folder);
	
	FILE *f = fopen(source, "rb");
	if(f) 
	{
		dbg_printf("Found on SD: %s\n", folder);
		fclose(f);
		ret = 1;
	}
	else
	{
		snprintf(source, sizeof(source), "%s/games/%s/sys/boot.bin", dm_boot_drive, folder);
		f = fopen(source, "rb");
		if(f) 
		{
			dbg_printf("Found on SD: %s\n", folder);
			fclose(f);
			ret = 2;
		}
	}
	
	if (ret == 0) {
		snprintf(source, sizeof(source), "%s/games/%s/game.iso", wbfs_fs_drive, folder);
	
		FILE *f = fopen(source, "rb");
		if(f) 
		{
			dbg_printf("Found on HDD: %s\n", folder);
			fclose(f);
			ret = 1;
		}
		else
		{
			snprintf(source, sizeof(source), "%s/games/%s/sys/boot.bin", wbfs_fs_drive, folder);
			f = fopen(source, "rb");
			if(f) 
			{
				dbg_printf("Found on HDD: %s\n", folder);
				fclose(f);
				ret = 2;
			}
		}
	}
	return ret;
}

void DML_New_SetOptions(char *GamePath,char *CheatPath, char *NewCheatPath, bool cheats, bool debugger, u8 NMM, u8 nodisc, u8 LED, u8 DMLvideoMode, u8 W_SCREEN, u8 PHOOK, u8 V_PATCH)
{
	dbg_printf("DML: Launch game 'sd:/games/%s/game.iso' through memory (new method)\n", GamePath);

	DML_CFG *DMLCfg = (DML_CFG*)memalign(32, sizeof(DML_CFG));
	memset(DMLCfg, 0, sizeof(DML_CFG));

	DMLCfg->Magicbytes = 0xD1050CF6;
	if (CFG.dml == CFG_DM_2_2)
		DMLCfg->Version = 0x00000002;
	else
		DMLCfg->Version = 0x00000001;	
	
	if (V_PATCH == 1)
		DMLCfg->VideoMode |= DML_VID_FORCE;
	else if (V_PATCH == 2)
		DMLCfg->VideoMode |= DML_VID_DML_AUTO;
	else
		DMLCfg->VideoMode |= DML_VID_NONE;

	//DMLCfg->Config |= DML_CFG_ACTIVITY_LED; //Sorry but I like it lol, option will may follow
	//DMLCfg->Config |= DML_CFG_PADHOOK; //Makes life easier, l+z+b+digital down...

	if(GamePath != NULL)
	{
		if(DML_GameIsInstalled(GamePath) == 2)
			snprintf(DMLCfg->GamePath, sizeof(DMLCfg->GamePath), "/games/%s/", GamePath);
		else
			snprintf(DMLCfg->GamePath, sizeof(DMLCfg->GamePath), "/games/%s/game.iso", GamePath);
		DMLCfg->Config |= DML_CFG_GAME_PATH;
	}

	if(CheatPath != NULL && NewCheatPath != NULL && cheats)
	{
		char *ptr;
		if(strstr(CheatPath, dm_boot_drive) == NULL)
		{
			fsop_CopyFile(CheatPath, NewCheatPath);
			ptr = &NewCheatPath[strlen(dm_boot_drive)];
		}
		else
			ptr = &CheatPath[strlen(dm_boot_drive)];
		strncpy(DMLCfg->CheatPath, ptr, sizeof(DMLCfg->CheatPath));
		DMLCfg->Config |= DML_CFG_CHEAT_PATH;
	}

	if(cheats)
	{
		DMLCfg->Config |= DML_CFG_CHEATS;
		printf_x(gt("Ocarina: Searching codes..."));
		printf("\n");
		sleep(1);
	}
	if(debugger)
		DMLCfg->Config |= DML_CFG_DEBUGGER;
	if(NMM > 0)
		DMLCfg->Config |= DML_CFG_NMM;
	if(NMM > 1)
		DMLCfg->Config |= DML_CFG_NMM_DEBUG;
	if(nodisc > 0 && CFG.dml >= CFG_DM_2_2)
		DMLCfg->Config |= DML_CFG_NODISC;
	else if (nodisc > 0)
		DMLCfg->Config |= DML_CFG_FORCE_WIDE;
	if(LED > 0)
		DMLCfg->Config |= DML_CFG_ACTIVITY_LED;
	if(W_SCREEN > 0)
		DMLCfg->Config |= DML_CFG_FORCE_WIDE; 
	if(PHOOK > 0)  
		DMLCfg->Config |= DML_CFG_PADHOOK;
	
	if(DMLvideoMode == 1 && V_PATCH == 1)
		DMLCfg->VideoMode |= DML_VID_FORCE_PAL50;
	else if(DMLvideoMode == 2 && V_PATCH == 1)
		DMLCfg->VideoMode |= DML_VID_FORCE_NTSC;
	else if(DMLvideoMode == 3 && V_PATCH == 1)
		DMLCfg->VideoMode |= DML_VID_FORCE_PAL60;
	else if(DMLvideoMode >= 4 && V_PATCH == 1)
		DMLCfg->VideoMode |= DML_VID_FORCE_PROG;


	if(DMLvideoMode > 3)
		DMLCfg->VideoMode |= DML_VID_PROG_PATCH;


	memcpy((void *)0xC0001700, DMLCfg, sizeof(DML_CFG));
	// For new DML v1.2+
	memcpy((void *)0xC1200000, DMLCfg, sizeof(DML_CFG));
	
	free(DMLCfg);
}

void DML_Old_SetOptions(char *GamePath, char *CheatPath, char *NewCheatPath, bool cheats)
{
	dbg_printf("DML: Launch game 'sd:/games/%s/game.iso' through boot.bin (old method)\n", GamePath);
	FILE *f;
	f = fopen("sd:/games/boot.bin", "wb");
	fwrite(GamePath, 1, strlen(GamePath) + 1, f);
	fclose(f);

	if(cheats && strstr(CheatPath, NewCheatPath) == NULL)
		fsop_CopyFile(CheatPath, NewCheatPath);

	//Tell DML to boot the game from sd card
	*(vu32*)0x80001800 = 0xB002D105;
	DCFlushRange((void *)(0x80001800), 4);
	ICInvalidateRange((void *)(0x80001800), 4);

	*(vu32*)0xCC003024 |= 7;
}

void DML_New_SetBootDiscOption(char *CheatPath, char *NewCheatPath, bool cheats, u8 NMM, u8 LED, u8 DMLvideoMode)
{
	dbg_printf("Booting GC game\n");

	DML_CFG *DMLCfg = (DML_CFG*)malloc(sizeof(DML_CFG));
	memset(DMLCfg, 0, sizeof(DML_CFG));

	DMLCfg->Magicbytes = 0xD1050CF6;
	
	if (CFG.dml == CFG_DML_1_2)
	DMLCfg->Version = 0x00000001;
	else
	DMLCfg->Version = 0x00000002;	
	
	DMLCfg->VideoMode |= DML_VID_DML_AUTO;
	DMLCfg->Config |= DML_CFG_PADHOOK;
	DMLCfg->Config |= DML_CFG_BOOT_DISC;
	
	if(CheatPath != NULL && NewCheatPath != NULL && cheats)
	{
		char *ptr;
		if(strstr(CheatPath, dm_boot_drive) == NULL)
		{
			fsop_CopyFile(CheatPath, NewCheatPath);
			ptr = &NewCheatPath[strlen(dm_boot_drive)];
		}
		else
			ptr = &CheatPath[strlen(dm_boot_drive)];
		strncpy(DMLCfg->CheatPath, ptr, sizeof(DMLCfg->CheatPath));
		DMLCfg->Config |= DML_CFG_CHEAT_PATH;
	}	

	if(cheats)
	{
		DMLCfg->Config |= DML_CFG_CHEATS;
		printf("cheat path=%s\n",DMLCfg->CheatPath);
		printf_x(gt("Ocarina: Searching codes..."));
		printf("\n");
		sleep(1);
	}
	
	if(NMM > 0)
		DMLCfg->Config |= DML_CFG_NMM;
	if(NMM > 1)
		DMLCfg->Config |= DML_CFG_NMM_DEBUG;
	if(LED > 0)
		DMLCfg->Config |= DML_CFG_ACTIVITY_LED;

	if(DMLvideoMode > 3)
		DMLCfg->VideoMode |= DML_VID_PROG_PATCH;
 
	//DML v1.2+
	memcpy((void *)0xC1200000, DMLCfg, sizeof(DML_CFG));

	free(DMLCfg);
}

s32 DML_write_size_info_file(struct discHdr *header, u64 size) {
	char filepath[0xFF];
	FILE *infoFile = NULL;
	
	if (header->magic == GC_GAME_ON_DM_DRIVE) {
		snprintf(filepath, sizeof(filepath), "%s/games/%s/size.bin", dm_boot_drive, header->folder);
	} else if (header->magic == GC_GAME_ON_GAME_DRIVE) {
		snprintf(filepath, sizeof(filepath), "%s/games/%s/size.bin", wbfs_fs_drive, header->folder);
	} else if (header->magic == GC_GAME_ON_SD_DRIVE) {
		snprintf(filepath, sizeof(filepath), "sd:/games/%s/size.bin", header->folder);
	}
	
	infoFile = fopen(filepath, "wb");
	fwrite(&size, 1, sizeof(u64), infoFile);
	fclose(infoFile);
	return 0;
}

u64 DML_read_size_info_file(struct discHdr *header) {
	char filepath[0xFF];
	FILE *infoFile = NULL;
	u64 result = 0;
	
	if (header->magic == GC_GAME_ON_DM_DRIVE) {
		snprintf(filepath, sizeof(filepath), "%s/games/%s/size.bin", dm_boot_drive, header->folder);
	} else if (header->magic == GC_GAME_ON_GAME_DRIVE) {
		snprintf(filepath, sizeof(filepath), "%s/games/%s/size.bin", wbfs_fs_drive, header->folder);
	} else if (header->magic == GC_GAME_ON_SD_DRIVE) {
		snprintf(filepath, sizeof(filepath), "sd:/games/%s/size.bin", header->folder);
	}
	
	infoFile = fopen(filepath, "rb");
	if (infoFile) {
		fread(&result, 1, sizeof(u64), infoFile);
		fclose(infoFile);
	}
	return result;
}

u64 getDMLGameSize(struct discHdr *header) {
	u64 result = 0;
	if (header->magic == GC_GAME_ON_DM_DRIVE)
	{
		char filepath[0xFF];
		snprintf(filepath, sizeof(filepath), "%s/games/%s/game.iso", dm_boot_drive, header->folder);
		FILE *fp = fopen(filepath, "r");
		if (!fp)
		{
			snprintf(filepath, sizeof(filepath), "%s/games/%s/sys/boot.bin", dm_boot_drive, header->folder);
			FILE *fp = fopen(filepath, "r");
			if (!fp)
				return result;
			fclose(fp);
			result = DML_read_size_info_file(header);
			if (result > 0)
				return result;
			snprintf(filepath, sizeof(filepath), "%s/games/%s/root/", dm_boot_drive, header->folder);
			result = fsop_GetFolderBytes(filepath);
			if (result > 0)
				DML_write_size_info_file(header, result);
		}
		else
		{
			fseek(fp, 0, SEEK_END);
			result = ftell(fp);
			fclose(fp);
		}
		return result;
	}
	else if (header->magic == GC_GAME_ON_GAME_DRIVE) 
	{
		char filepath[0xFF];
		sprintf(filepath, "%s/games/%s/game.iso", wbfs_fs_drive, header->folder);
		FILE *fp = fopen(filepath, "r");
		if (!fp)
		{
			snprintf(filepath, sizeof(filepath), "%s/games/%s/sys/boot.bin", wbfs_fs_drive, header->folder);
			FILE *fp = fopen(filepath, "r");
			if (!fp)
				return result;
			fclose(fp);
			result = DML_read_size_info_file(header);
			if (result > 0)
				return result;
			snprintf(filepath, sizeof(filepath), "%s/games/%s/root/", wbfs_fs_drive, header->folder);
			result = fsop_GetFolderBytes(filepath);
			if (result > 0)
				DML_write_size_info_file(header, result);
		}
		else
		{
			fseek(fp, 0, SEEK_END);
			result = ftell(fp);
			fclose(fp);
		}
		return result;
	}
	else if (header->magic == GC_GAME_ON_SD_DRIVE) 
	{
		char filepath[0xFF];
		sprintf(filepath, "sd:/games/%s/game.iso", header->folder);
		FILE *fp = fopen(filepath, "r");
		if (!fp)
		{
			snprintf(filepath, sizeof(filepath), "sd:/games/%s/sys/boot.bin", header->folder);
			FILE *fp = fopen(filepath, "r");
			if (!fp)
				return result;
			fclose(fp);
			result = DML_read_size_info_file(header);
			if (result > 0)
				return result;
			snprintf(filepath, sizeof(filepath), "sd:/games/%s/root/", header->folder);
			result = fsop_GetFolderBytes(filepath);
			if (result > 0)
				DML_write_size_info_file(header, result);
		}
		else
		{
			fseek(fp, 0, SEEK_END);
			result = ftell(fp);
			fclose(fp);
		}
		return result;
	}
	return result;
}

s32 delete_Old_Copied_DML_Game() {
	FILE *infoFile = NULL;
	struct discHdr header;
	char infoFilePath[255];
	sprintf(infoFilePath, "%s/game/lastCopied.bin", dm_boot_drive);
	infoFile = fopen(infoFilePath, "rb");
	if (infoFile) {
		fread(&header, 1, sizeof(struct discHdr), infoFile);
		fclose(infoFile);
		DML_RemoveGame(header, true);
		return 0;
	}
	return -1;
}

s32 copy_DML_Game_to_SD(struct discHdr *header) {
	char source[255];
	char target[255];
	if (header->magic == GC_GAME_ON_GAME_DRIVE) {
		sprintf(source, "%s/games/%s", wbfs_fs_drive, header->folder);
	} else if (header->magic == GC_GAME_ON_SD_DRIVE) {
		sprintf(source, "sd:/games/%s", header->folder);
	}
	
	sprintf(target, "%s/games/%s", dm_boot_drive, header->folder);
	fsop_CopyFolder(source, target);
	header->magic = GC_GAME_ON_DM_DRIVE;
	
	FILE *infoFile = NULL;
	char infoFilePath[255];
	sprintf(infoFilePath, "%s/game/lastCopied.bin", dm_boot_drive);
	infoFile = fopen(infoFilePath, "wb");
	fwrite((u8*)header, 1, sizeof(struct discHdr), infoFile);
	fclose(infoFile);
	return 0;
}

// Devolution
static gconfig *DEVO_CONFIG = (gconfig*)0x80000020;

void DEVO_SetOptions(const char *path, u8 NMM)
{
	struct stat st;
	char full_path[256];
	int data_fd;

	stat(path, &st);
	u8 *lowmem = (u8*)0x80000000;
	FILE *iso_file = fopen(path, "rb");
	if(iso_file)
		{
		printf("path=%s, iso found !!\n",path);
		}
	fread(lowmem, 1, 32, iso_file);
	fclose(iso_file);

	// fill out the Devolution config struct
	memset(DEVO_CONFIG, 0, sizeof(*DEVO_CONFIG));
	DEVO_CONFIG->signature = 0x3EF9DB23;
	DEVO_CONFIG->version = 0x00000110;
	DEVO_CONFIG->device_signature = st.st_dev;
	DEVO_CONFIG->disc1_cluster = st.st_ino;
	
	// For 2nd ios file
	struct stat st2;
	char disc2path[256];
	strcpy(disc2path, path);
	char *posz = (char *)NULL;
	posz = strstr(disc2path, "game.iso");
	if(posz != NULL)				
		strncpy(posz, "gam2.iso", 8);
		
	iso_file = fopen(disc2path, "rb");
	if(iso_file)
	{
		printf("path=%s, iso found !!\n",disc2path);
		//sleep(5);
		stat(disc2path, &st2);
		fread(lowmem, 1, 32, iso_file);
		fclose(iso_file);	
		DEVO_CONFIG->disc2_cluster = st2.st_ino;	
	}

	// make sure these directories exist, they are required for Devolution to function correctly
	snprintf(full_path, sizeof(full_path), "usb:/apps");
	fsop_MakeFolder(full_path);
	snprintf(full_path, sizeof(full_path), "usb:/apps/gc_devo");
	fsop_MakeFolder(full_path);

	// find or create a 16MB memcard file for emulation
	// this file can be located anywhere since it's passed by cluster, not name
	// it must be at least 16MB though
	snprintf(full_path, sizeof(full_path), "%s/memcard.bin",USBLOADER_PATH);

	// check if file doesn't exist or is less than 16MB
	if(stat(full_path, &st) == -1 || st.st_size < 16<<20)
	{
		// need to enlarge or create it
		data_fd = open(full_path, O_WRONLY|O_CREAT);
		if (data_fd>=0)
		{
			// make it 16MB
			printf("Resizing memcard file...\n");
			ftruncate(data_fd, 16<<20);
			if (fstat(data_fd, &st)==-1 || st.st_size < 16<<20)
			{
				// it still isn't big enough. Give up.
				st.st_ino = 0;
			}
			close(data_fd);
		}
		else
		{
			// couldn't open or create the memory card file
			st.st_ino = 0;
		}
	}

	// set FAT cluster for start of memory card file
	// if this is zero memory card emulation will not be used
	if(!NMM > 0)
		{
		st.st_ino = 0;
	printf("emu. memcard is disabled\n");
	  }
	  else
	  printf("emu. memcard is enabled\n");
	DEVO_CONFIG->memcard_cluster = st.st_ino;

	// flush disc ID and Devolution config out to memory
	DCFlushRange(lowmem, 64);
}