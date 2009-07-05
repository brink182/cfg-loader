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

#include "patchcode.h"
#include "kenobiwii.h"

#define FSTDIRTYPE 1
#define FSTFILETYPE 0
#define ENTRYSIZE 0xC
//#define FILEDIR	"fat0:/codes"
//#define FILEDIR	"sd:/codes"
#define FILEDIR	"/codes"

#define MAX_FILENAME_LEN	128


static vu32 dvddone = 0;


// Real basic 
u32 do_sd_code(char *filename)
{
	FILE *fp;
	u8 *filebuff;
	u32 filesize;
	u32 ret;
	u32 ret_val = 0;
	char filepath[128];
	
	/*
	// fat already initialized in main()
	ret = fatInitDefault();
	if (!ret) {
		printf("[+] SD Error\n");
		sleep (2);
		return 0;
	}
	*/

	fflush(stdout);
	
	//snprintf(filepath, sizeof(filepath), "%s/%s.gct", FILEDIR, filename);
	snprintf(filepath, sizeof(filepath), "%s%s/%s.gct", FAT_DRIVE, FILEDIR, filename);

	//printf("filename %s\n",filepath);
	
	fp = fopen(filepath, "rb");
	if (!fp) {
		printf("[+] Ocarina: No SD codes found\n");
		sleep(2);
		return 0;
	}

	fseek(fp, 0, SEEK_END);
	filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	filebuff = (u8*) malloc (filesize);
	if(filebuff == 0){
		fclose(fp);
		sleep(2);
		return 0;
	}

	ret = fread(filebuff, 1, filesize, fp);
	if(ret != filesize){	
		printf("[+] Ocarina: SD Code Error\n");
		free(filebuff);
		fclose(fp);
		sleep(2);
		return 0;
	}
    printf("[+] Ocarina: SD Codes found.\n");

	// ocarina config options are done elswhere, confirmation optional
	if (!CFG.confirm_ocarina) goto no_confirm;

	printf("    Press A button to apply codes.\n");
	printf("    Press B button to skip codes.\n");
	/* Wait for user answer */
	for (;;) {
		u32 buttons = Wpad_WaitButtons();

		/* A button */
		if (buttons & WPAD_BUTTON_A) {
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
		if (buttons & WPAD_BUTTON_B)
				break;
	}

	free(filebuff);
	fclose(fp);
	sleep(2);
	return ret_val;
}


