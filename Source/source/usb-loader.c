#include <stdio.h>
#include <stdlib.h>
#include <ogcsys.h>
#include <unistd.h>

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


int main(int argc, char **argv)
{
	s32 ret;

	#if DEFAULT_IOS_IDX == CFG_IOS_249

	/* Load Custom IOS */
	ret = IOS_ReloadIOS(249);
	CURR_IOS_IDX = CFG_IOS_249;

	#else

	/* Load Custom IOS */
	cfg_ios_set_idx(DEFAULT_IOS_IDX);
	ret = ReloadIOS(0, 0);

	#endif

	/* Initialize system */
	Sys_Init();

	/* Initialize subsystems */
	//Subsystem_Init();
	// delay wpad_init after second reloadIOS
	Fat_MountSDHC();

	/* Load configuration */
	CFG_Load(argc, argv);

	if (CURR_IOS_IDX != CFG.game.ios_idx) {
		usleep(300000);
		ret = ReloadIOS(0, 0);
		usleep(300000);
	}

	/* Set video mode */
	Video_SetMode();

	// Gui Init
	Gui_Init();

	/* Show background */
	Gui_DrawBackground();

	/* Initialize console */
	Gui_InitConsole();

	/* Initialize Wiimote subsystem */
	Wpad_Init();
	
	/* Set up config parameters */
	CFG_Setup(argc, argv);

	/* Check if Custom IOS is loaded */
	if (ret < 0) {
		printf("[+] ERROR:\n");
		printf("    Custom IOS %d could not be loaded! (%d)\n", CFG.ios, ret);

		goto out;
	}
	// Notify if ios was reloaded by cfg
	if (CURR_IOS_IDX != DEFAULT_IOS_IDX) {
		printf("\n[+] Custom IOS %s Loaded OK\n\n", ios_str(CURR_IOS_IDX));
		sleep(2);
	}

	/* Initialize DIP module */
	ret = Disc_Init();
	if (ret < 0) {
		printf("[+] ERROR:\n");
		printf("    Could not initialize DIP module! (ret = %d)\n", ret);

		goto out;
	}

	/* Menu loop */
	Menu_Loop();

out:
	sleep(5);
	/* Restart */
	Restart_Wait();

	return 0;
}

