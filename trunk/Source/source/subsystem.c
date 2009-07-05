#include <stdio.h>
#include <ogcsys.h>

#include "fat.h"
#include "wpad.h"
#include "music.h"

extern void Net_Close();

void Subsystem_Init(void)
{
	/* Initialize Wiimote subsystem */
	Wpad_Init();

	/* Mount SDHC */
	Fat_MountSDHC();
}

void Subsystem_Close(void)
{
	// stop music
	Music_Stop();

	// stop gui
	extern void Gui_Close();
	Gui_Close();

	// close network
	Net_Close();

	/* Disconnect Wiimotes */
	Wpad_Disconnect();

	/* Unmount SDHC */
	Fat_UnmountSDHC();
	Fat_UnmountUSB();
}

