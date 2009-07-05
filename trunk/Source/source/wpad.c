#include <stdio.h>
#include <ogcsys.h>
#include <time.h>

#include "sys.h"
#include "wpad.h"
#include "subsystem.h"

/* Constants */
#define MAX_WIIMOTES	4

extern u8 shutdown;
void __Wpad_PowerCallback(s32 chan)
{
	/* Poweroff console */
	shutdown = 1;
}


s32 Wpad_Init(void)
{
	s32 ret;

	/* Initialize Wiimote subsystem */
	ret = WPAD_Init();
	if (ret < 0)
		return ret;

	/* Set POWER button callback */
	WPAD_SetPowerButtonCallback(__Wpad_PowerCallback);

	WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);

	return ret;
}

void Wpad_Disconnect(void)
{
	u32 cnt;

	/* Disconnect Wiimotes */
	for (cnt = 0; cnt < MAX_WIIMOTES; cnt++)
		WPAD_Disconnect(cnt);

	/* Shutdown Wiimote subsystem */
	WPAD_Shutdown();
}

u32 Wpad_GetButtons(void)
{
	u32 buttons = 0, cnt;

	/* Scan pads */
	WPAD_ScanPads();

	// handle shutdown
	if (shutdown) {
		Subsystem_Close();
		Sys_Shutdown();
	}

	/* Get pressed buttons */
	for (cnt = 0; cnt < MAX_WIIMOTES; cnt++)
		buttons |= WPAD_ButtonsDown(cnt);

	return buttons;
}

u32 Wpad_WaitButtons(void)
{
	u32 buttons = 0;

	/* Wait for button pressing */
	while (!buttons) {
		buttons = Wpad_GetButtons();
		VIDEO_WaitVSync();
	}

	return buttons;
}

extern long long gettime();
extern u32 diff_msec(long long start,long long end);

u32 Wpad_WaitButtonsRpt(void)
{
	static int held = 0;
	static long long t_start;

	u32 buttons = 0;
	u32 buttons_held = 0;
	int cnt;
	long long t_now;
	unsigned wait;
	unsigned ms_diff;

	/* Wait for button pressing */
	while (!buttons) {
		if (shutdown) {
			Subsystem_Close();
			Sys_Shutdown();
		}
		buttons = Wpad_GetButtons();
		if (buttons) {
			held = 0;
			if (buttons & (WPAD_BUTTON_UP | WPAD_BUTTON_DOWN)) {
				held = 1;
				t_start = gettime();
			}
		} else if (held) {
			buttons_held = 0;
			for (cnt = 0; cnt < MAX_WIIMOTES; cnt++) {
				buttons_held |= WPAD_ButtonsHeld(cnt);
			}
			if (buttons_held & (WPAD_BUTTON_UP | WPAD_BUTTON_DOWN)) {
				t_now = gettime();
				ms_diff = diff_msec(t_start, t_now);
				if (held == 1) wait = 300; else wait = 150;
				if (ms_diff > wait) {
					if (buttons_held & WPAD_BUTTON_UP) {
						buttons |= WPAD_BUTTON_UP;
					} else if (buttons_held & WPAD_BUTTON_DOWN) {
						buttons |= WPAD_BUTTON_DOWN;
					}
					t_start = t_now;
					held = 2;
				}
			} else {
				held = 0;
			}
		}
		VIDEO_WaitVSync();
	}

	return buttons;
}


