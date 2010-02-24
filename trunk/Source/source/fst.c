/*
 *  Copyright (C) 2008 Nuke (wiinuke@gmail.com)
 *
 *  this file is part of GeckoOS for USB Gecko
 *  http://www.usbgecko.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <malloc.h>
#include <sys/unistd.h>
#include <fat.h>
#include "ogc/ipc.h"
#include "fst.h"
#include "dvd_broadway.h"
#include "wpad.h"
#include "cfg.h"
#include "fat.h"
#include "menu.h"
#include "video.h"

#include "patchcode.h"
#include "kenobiwii.h"
#include "gettext.h"

#define FSTDIRTYPE 1
#define FSTFILETYPE 0
#define ENTRYSIZE 0xC
//#define FILEDIR	"fat0:/codes"
//#define FILEDIR	"sd:/codes"
#define FILEDIR	"/codes"

#define MAX_FILENAME_LEN	128


//static vu32 dvddone = 0;

#if 0
// Real basic 
u32 do_sd_code(char *filename)
{
	FILE *fp;
	u8 *filebuff;
	u32 filesize;
	u32 ret;
	u32 ret_val = 0;
	char filepath[128];
	char *fat_drive = FAT_DRIVE;
	int i;
	
	/*
	// fat already initialized in main()
	ret = fatInitDefault();
	if (!ret) {
		printf_x("SD Error\n");
		sleep (2);
		return 0;
	}
	*/

	fflush(stdout);
	
	printf_x(gt("Ocarina: Searching codes..."));
	printf("\n");

	for (i=0; i<3; i++) {
		switch (i) {
		case 0:
			snprintf(filepath, sizeof(filepath), "%s%s/%s.gct",
				USBLOADER_PATH, FILEDIR, filename);
			break;
		case 1:
			snprintf(filepath, sizeof(filepath), "%s/data/gecko%s/%s.gct",
				fat_drive, FILEDIR, filename);
			break;
		case 2:
			snprintf(filepath, sizeof(filepath), "%s%s/%s.gct",
				fat_drive, FILEDIR, filename);
			break;
		}
		printf("    %s\n", filepath);
		fp = fopen(filepath, "rb");
		if (fp) break;
		if (i == 2 && strcmp(fat_drive, "usb:") == 0) {
			// retry on sd:
			fat_drive = "sd:";
			i = 0;
		}
	}
	if (!fp) {
		printf_x(gt("Ocarina: No codes found"));
		printf("\n");
		sleep(3);
		return 0;
	}

	fseek(fp, 0, SEEK_END);
	filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	filebuff = (u8*) malloc (filesize);
	if(filebuff == 0){
		printf_x(gt("Ocarina: Out Of Memory Error"));
		printf("\n");
		fclose(fp);
		sleep(2);
		return 0;
	}

	ret = fread(filebuff, 1, filesize, fp);
	if(ret != filesize){	
		printf_x(gt("Ocarina: Code Error"));
		printf("\n");
		free(filebuff);
		fclose(fp);
		sleep(2);
		return 0;
	}
    printf_x(gt("Ocarina: Codes found."));
	printf("\n");

	// ocarina config options are done elswhere, confirmation optional
	if (!CFG.confirm_ocarina) goto no_confirm;

	printf("    ");
	printf(gt("Press %s button to apply codes."), (button_names[CFG.button_confirm.num]));
	printf("\n    ");
	printf(gt("Press %s button to skip codes."), (button_names[CFG.button_cancel.num]));
	printf("\n");
	/* Wait for user answer */
	for (;;) {
		u32 buttons = Wpad_WaitButtons();

		/* A button */
		if (buttons & CFG.button_confirm.mask) {
			no_confirm:
			// HOOKS STUFF - FISHEARS
			if (CFG.game.ocarina) {
				memset((void*)0x80001800,0,kenobiwii_size);
				memcpy((void*)0x80001800,kenobiwii,kenobiwii_size);
				DCFlushRange((void*)0x80001800,kenobiwii_size);
				hooktype = 1;
				memcpy((void*)0x80001800, (char*)0x80000000, 6);	// For WiiRD
			}
			// copy codes over
			memcpy((void*)0x800027E8,filebuff,filesize);
			// enable flag
			*(vu8*)0x80001807 = 0x01;
			// hooks are patched in dogamehooks()
			ret_val = 1;
			break;
		}

		/* B button */
		if (buttons & CFG.button_cancel.mask)
				break;
	}

	free(filebuff);
	fclose(fp);
	sleep(2);
	return ret_val;
}

#endif

u8 *code_buf = NULL;
int code_size = 0;

int ocarina_load_code(u8 *id)
{
	char filename[8];
	char filepath[128];
	char *fat_drive = FAT_DRIVE;
	int i;
	memset(filename, 0, sizeof(filename));
	memcpy(filename, id, 6);
	
	printf_x(gt("Ocarina: Searching codes..."));
	printf("\n");

	for (i=0; i<3; i++) {
		switch (i) {
		case 0:
			snprintf(filepath, sizeof(filepath), "%s%s/%s.gct",
				USBLOADER_PATH, FILEDIR, filename);
			break;
		case 1:
			snprintf(filepath, sizeof(filepath), "%s/data/gecko%s/%s.gct",
				fat_drive, FILEDIR, filename);
			break;
		case 2:
			snprintf(filepath, sizeof(filepath), "%s%s/%s.gct",
				fat_drive, FILEDIR, filename);
			break;
		}
		printf_("%s\n", filepath);
		code_size = Fat_ReadFile(filepath, (void*)(&code_buf));
		if (code_size > 0 && code_buf) break;
		SAFE_FREE(code_buf);
		code_size = 0;
		if (i == 2 && strcmp(fat_drive, "usb:") == 0) {
			// retry on sd:
			fat_drive = "sd:";
			i = 0;
		}
	}
	if (!code_buf) {
		printf_x(gt("Ocarina: No codes found"));
		printf("\n");
		sleep(2);
		return 0;
	}

    printf_x(gt("Ocarina: Codes found."));
	printf("\n");

	// optional confirmation
	if (CFG.confirm_ocarina) {
		Gui_Console_Enable();
		printf_h(gt("Press %s button to apply codes."), (button_names[CFG.button_confirm.num]));
		printf("\n");
		printf_h(gt("Press %s button to skip codes."), (button_names[CFG.button_cancel.num]));
		printf("\n");
		/* Wait for user answer */
		for (;;) {
			u32 buttons = Wpad_WaitButtons();
			if (buttons & CFG.button_confirm.mask) break;
			if (buttons & CFG.button_cancel.mask) {
				SAFE_FREE(code_buf);
				code_size = 0;
				break;
			}
		}
	}

	return code_size;
}

int ocarina_do_code()
{
	if (!CFG.game.ocarina) return 0;
	if (!code_buf) {
		if (usb_isgeckoalive(EXI_CHANNEL_1)){
			printf_(gt("USB Gecko found. Debugging is enabled."));
			printf_("\n");
		} else {
			return 0;
		}
	}
	// HOOKS STUFF - FISHEARS
	memset((void*)0x80001800,0,kenobiwii_size);
	memcpy((void*)0x80001800,kenobiwii,kenobiwii_size);
	DCFlushRange((void*)0x80001800,kenobiwii_size);
	hooktype = 1;
	memcpy((void*)0x80001800, (char*)0x80000000, 6);	// For WiiRD
	// copy codes over
	memcpy((void*)0x800027E8,code_buf,code_size);
	// enable flag
	*(vu8*)0x80001807 = 0x01;
	// hooks are patched in dogamehooks()
	return 1;
}

