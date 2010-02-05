#include <stdio.h>
#include <stdlib.h>
#include <ogcsys.h>
#include <unistd.h>
#include <string.h>

#include "disc.h"
#include "gui.h"
#include "menu.h"
#include "restart.h"
#include "subsystem.h"
#include "sys.h"
#include "video.h"
#include "cfg.h"
#include "wpad.h"
#include "fat.h"
#include "util.h"

extern int __console_disable;

void print_ios()
{
	if (!(CFG.direct_launch && !CFG.intro)) {
		__console_disable = 0;
	}
	if ( (strncasecmp(CFG.partition, "fat", 3) == 0)
		&& (CFG.game.ios_idx == CFG_IOS_222_MLOAD
			|| CFG.game.ios_idx == CFG_IOS_223_MLOAD) ) {
		printf("\n%d-fat", CFG.ios);
	} else {
		printf("\n%s", ios_str(CFG.game.ios_idx));
	}
	__console_flush(0);
	__console_disable = 1;
}

int main(int argc, char **argv)
{
	s32 ret;

	/* Initialize system */
	mem_init();
	Sys_Init();

	cfg_parsearg_early(argc, argv);

	/* Set video mode */
	Video_SetMode();

	Gui_DrawIntro();
	__console_disable = 1;

	//CFG.debug = 1;
	/*if (CFG.debug) {
		__console_disable = 0;
		Gui_InitConsole();
		dbg_printf("Loading IOS%s...", ios_str(CFG.game.ios_idx));
	}*/
	
	/* Load Custom IOS */
	if (CFG.game.ios_idx == CFG_IOS_249) {
		ret = IOS_ReloadIOS(249);
		CURR_IOS_IDX = CFG_IOS_249;
		//usleep(200000); // this seems to cause hdd spin down/up
	} else {
		print_ios();
		ret = ReloadIOS(0, 0);
	}
	//dbg_printf("(%d)\n", ret);
	//__console_disable = 1;

	/* Initialize subsystems */
	//Subsystem_Init();
	// delay wpad_init after second reloadIOS
	//dbg_printf("Fat_MountSDHC\n");
	Fat_MountSDHC();

	/* Load configuration */
	//dbg_printf("CFG_Load\n");
	CFG_Load(argc, argv);

	if (CURR_IOS_IDX != CFG.game.ios_idx) {
		print_ios();
		usleep(300000);
		ret = ReloadIOS(0, 0);
		usleep(300000);
	}

   	// set widescreen as set by CFG.widescreen
	Video_SetWide();

	util_init();

	// Gui Init
	Gui_Init();

	// Show background
	//Gui_DrawBackground();
	Gui_LoadBackground();

	/* Initialize console */
	Gui_InitConsole();

	// debug:
	//__console_disable = 1;
	//Gui_Console_Enable();

	/* Initialize Wiimote subsystem */
	Wpad_Init();

	/* Set up config parameters */
	//dbg_printf("CFG_Setup\n"); //Wpad_WaitButtons();
	CFG_Setup(argc, argv);

	/* Check if Custom IOS is loaded */
	if (ret < 0) {
		Gui_Console_Enable();
		printf("[+] ERROR:\n");
		printf("    Custom IOS %d could not be loaded! (%d)\n", CFG.ios, ret);
		goto out;
	}
	
	// Notify if ios was reloaded by cfg
	if (CURR_IOS_IDX != DEFAULT_IOS_IDX) {
		//Gui_Console_Enable();
		printf("\n[+] Custom IOS %s Loaded OK\n\n", ios_str(CURR_IOS_IDX));
		sleep(2);
	}
	
	/* Initialize DIP module */
	ret = Disc_Init();
	if (ret < 0) {
		Gui_Console_Enable();
		printf("[+] ERROR:\n");
		printf("    Could not initialize DIP module! (ret = %d)\n", ret);
		goto out;
	}

	/* Menu loop */
	//dbg_printf("Menu_Loop\n"); //Wpad_WaitButtons();
	Menu_Loop();

out:
	sleep(5);
	/* Restart */
	Restart_Wait();

	return 0;
}

