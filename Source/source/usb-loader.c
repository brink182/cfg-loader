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
#include "gettext.h"
#include "mload.h"

extern int __console_disable;

void print_ios()
{
	printf("%*s", 62, "");
	/*if ( (strncasecmp(CFG.partition, "fat", 3) == 0) && CFG.ios_mload ) {
		printf("%d-fat", CFG.ios);
	} else {*/
		printf("%s", ios_str(CFG.game.ios_idx));
	//}
	__console_flush(0);
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

	dbg_printf("reaload ios: %d\n", CFG.ios);
	/* Load Custom IOS */
	print_ios();
	if (CFG.game.ios_idx == CFG_IOS_249) {
		ret = IOS_ReloadIOS(249);
		CURR_IOS_IDX = CFG_IOS_249;
		if (is_ios_type(IOS_TYPE_WANIN) && IOS_GetRevision() >= 18) {
			load_dip_249();
			//try_hello();
		}
		//usleep(200000); // this seems to cause hdd spin down/up
	} else {
		ret = ReloadIOS(0, 0);
	}
	printf("\n");
	dbg_printf(" = %d\n", ret);

	/* Initialize subsystems */
	//Subsystem_Init();
	// delay wpad_init after second reloadIOS
	dbg_printf("Fat Mount SD\n");
	Fat_MountSDHC();

	//save_dip();

	/* Load configuration */
	dbg_printf("CFG Load\n");
	CFG_Load(argc, argv);

	if (CURR_IOS_IDX != CFG.game.ios_idx) {
		print_ios();
		usleep(300000);
		ret = ReloadIOS(0, 0);
		usleep(300000);
		printf("\n");
	}

	/* Check if Custom IOS is loaded */
	if (ret < 0) {
		__console_disable = 0;
		printf_x(gt("ERROR:"));
		printf("\n");
		printf_(gt("Custom IOS %d could not be loaded! (ret = %d)"), CFG.ios, ret);
		printf("\n");
		goto out;
	}
	
	__console_disable = 1;

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

	/* Initialize Wiimote subsystem */
	Wpad_Init();

	/* Set up config parameters */
	//printf("CFG_Setup\n"); //Wpad_WaitButtons();
	CFG_Setup(argc, argv);

	// Notify if ios was reloaded by cfg
	if (CURR_IOS_IDX != DEFAULT_IOS_IDX) {
		printf("\n");
		printf_x(gt("Custom IOS %s Loaded OK"), ios_str(CURR_IOS_IDX));
		printf("\n\n");
		sleep(2);
	}
	
	/* Initialize DIP module */
	ret = Disc_Init();
	if (ret < 0) {
		Gui_Console_Enable();
		printf_x(gt("ERROR:"));
		printf("\n");
		printf_(gt("Could not initialize DIP module! (ret = %d)"), ret);
		printf("\n");
		goto out;
	}

	/* Menu loop */
	//dbg_printf("Menu_Loop\n"); //Wpad_WaitButtons();
	Menu_Loop();

out:
	sleep(8);
	/* Restart */
	exit(0);
	Restart_Wait();

	return 0;
}

