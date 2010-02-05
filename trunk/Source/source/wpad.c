
// GC, classic controller, nunchuk and guitar support by gannon

#include <stdio.h>
#include <ogcsys.h>
#include <time.h>
#include <ogc/pad.h>
#include <math.h>

#include "sys.h"
#include "wpad.h"
#include "subsystem.h"

/* Constants */
#define MAX_WIIMOTES	4
#define MAX_BUTTONS	11
#define WIIMOTE	0
#define CLASSIC	1
#define GCPAD	2
#define GUITAR	3
//#define NUNCHUCK	4
#define MAX_X	660
#define MAX_Y	480
//#define PI	3.14159265f

extern u8 shutdown;
u32 buttonmap[4][MAX_BUTTONS];
float coord[2] = {320,240};
bool padMoved = false;

void Do_Shutdown() {
	Services_Close();
	Subsystem_Close();
	Sys_Shutdown();
}

s8 Wpad_Stick(int n, bool y) {

    float m = 0.0;
    float a = 0.0;
    double tmp;
    WPADData *data = WPAD_Data(n);

    if (data->exp.type == WPAD_EXP_NUNCHUK) {
        m = data->exp.nunchuk.js.mag;
        a = data->exp.nunchuk.js.ang;
    } else if (data->exp.type == WPAD_EXP_CLASSIC) {
        m = data->exp.classic.ljs.mag;
        a = data->exp.classic.ljs.ang;
    } else if (data->exp.type == WPAD_EXP_GUITARHERO3) {
        m = data->exp.gh3.js.mag;
        a = data->exp.gh3.js.ang;
    }

    if (m > 1.0) m = 1.0;
    else if (m < -1.0) m = -1.0;

    tmp = (y == 0) ? m * sin((M_PI * a)/180.0f) : m * cos((M_PI * a)/180.0f);
    return (s8)(tmp * 128.0f);
}

void Wpad_getIR(int n, struct ir_t *ir) {

    WPAD_ScanPads();
    PAD_ScanPads();
	// handle shutdown
	if (shutdown)
		Do_Shutdown();

    WPAD_IR(n, ir);
	ir->sx -= 160;
	ir->sy -= 220;
    if (ir->smooth_valid == 0) {
        s8 padX = PAD_StickX(n);
        s8 padY = PAD_StickY(n);
        s8 wpadX = Wpad_Stick(n, 0);
        s8 wpadY = Wpad_Stick(n, 1);
		float mX = ((padX+wpadX) / 8);
		float mY = ((padY+wpadY) / 8);
		if (mX > 1 || mY > 1 || mX < -1 || mY < -1)
			padMoved = true;
		if (padMoved) {
			coord[0] += mX;
			coord[1] -= mY;
			if (coord[0] < 0) coord[0] = 0;
			else if (coord[0] > MAX_X) coord[0] = MAX_X;
			if (coord[1] < 0) coord[1] = 0;
			else if (coord[1] > MAX_Y) coord[1] = MAX_Y;
			ir->sx = coord[0]-20;
			ir->sy = coord[1]-20;
			ir->angle = 0.0;
			ir->raw_valid = 1;
			ir->smooth_valid = 1;
			ir->valid = 1;
		}
    } else {
		//coord = {340,280};
		coord[0] = 320;
		coord[1] = 240;
		padMoved = false;
	}
	return;
}

u32 readPad(int n, bool held) {

	int i;
	u32 pad, wii, type;
	u32 buttons = 0; 
//	PADStatus status[4];
//	PAD_Read(status);
//	if (status[n].err == PAD_ERR_NONE) {
	pad = (held == 1) ? PAD_ButtonsHeld(n) : PAD_ButtonsDown(n);
	for (i=0; i<MAX_BUTTONS; i++) {
		if ((pad & buttonmap[GCPAD][i]))
			buttons |= buttonmap[WIIMOTE][i];
	}
//	} else
//		PAD_Rest(n);
	if (WPAD_Probe(n, &type) == WPAD_ERR_NONE) {
		wii = (held == 1) ? WPAD_ButtonsHeld(n) : WPAD_ButtonsDown(n);
		if (type == WPAD_EXP_NONE || type == WPAD_EXP_NUNCHUK) {
			buttons |= wii;
		} else if (type == WPAD_EXP_CLASSIC) {
			for (i=0; i<MAX_BUTTONS; i++) {
				if ((wii & buttonmap[CLASSIC][i]) || (wii & buttonmap[WIIMOTE][i]))
					buttons |= buttonmap[WIIMOTE][i];
			}
		} else if (type == WPAD_EXP_GUITARHERO3) {
			for (i=0; i<MAX_BUTTONS; i++) {
				if ((wii & buttonmap[GUITAR][i]) || (wii & buttonmap[WIIMOTE][i]))
					buttons |= buttonmap[WIIMOTE][i];
			}
		}

	}
	return buttons;
}

u32 Wpad_Held(int n)
{
	return readPad(n, 1);
}

void makeButtonMap(void) {

	buttonmap[GCPAD][0] = PAD_BUTTON_UP;
	buttonmap[WIIMOTE][0] = WPAD_BUTTON_UP;
	buttonmap[CLASSIC][0] = WPAD_CLASSIC_BUTTON_UP;
	buttonmap[GUITAR][0] = WPAD_GUITAR_HERO_3_BUTTON_STRUM_UP;
	//buttonmap[NUNCHUK][0] = WPAD_NUNCHUK_BUTTON_A;
	
	buttonmap[GCPAD][1] = PAD_BUTTON_RIGHT;
	buttonmap[WIIMOTE][1] = WPAD_BUTTON_RIGHT;
	buttonmap[CLASSIC][1] = WPAD_CLASSIC_BUTTON_RIGHT;
	buttonmap[GUITAR][1] = WPAD_BUTTON_RIGHT;
	//buttonmap[NUNCHUK][0] = WPAD_NUNCHUK_BUTTON_A;
	
	buttonmap[GCPAD][2] = PAD_BUTTON_DOWN;
	buttonmap[WIIMOTE][2] = WPAD_BUTTON_DOWN;
	buttonmap[CLASSIC][2] = WPAD_CLASSIC_BUTTON_DOWN;
	buttonmap[GUITAR][2] = WPAD_GUITAR_HERO_3_BUTTON_STRUM_DOWN;
	//buttonmap[NUNCHUK][0] = WPAD_NUNCHUK_BUTTON_A;
	
	buttonmap[GCPAD][3] = PAD_BUTTON_LEFT;
	buttonmap[WIIMOTE][3] = WPAD_BUTTON_LEFT;
	buttonmap[CLASSIC][3] = WPAD_CLASSIC_BUTTON_LEFT;
	buttonmap[GUITAR][3] = WPAD_BUTTON_LEFT;
	//buttonmap[NUNCHUK][3] = WPAD_NUNCHUK_BUTTON_A;
	
	buttonmap[GCPAD][4] = PAD_TRIGGER_L;
	buttonmap[WIIMOTE][4] = WPAD_BUTTON_MINUS;
	buttonmap[CLASSIC][4] = WPAD_CLASSIC_BUTTON_MINUS;
	buttonmap[GUITAR][4] = WPAD_GUITAR_HERO_3_BUTTON_MINUS;
	//buttonmap[NUNCHUK][0] = WPAD_NUNCHUK_BUTTON_A;
	
	buttonmap[GCPAD][5] = PAD_TRIGGER_R;
	buttonmap[WIIMOTE][5] = WPAD_BUTTON_PLUS;
	buttonmap[CLASSIC][5] = WPAD_CLASSIC_BUTTON_PLUS;
	buttonmap[GUITAR][5] = WPAD_GUITAR_HERO_3_BUTTON_PLUS;
	//buttonmap[NUNCHUK][0] = WPAD_NUNCHUK_BUTTON_A;
	
	buttonmap[GCPAD][6] = PAD_BUTTON_A;
	buttonmap[WIIMOTE][6] = WPAD_BUTTON_A;
	buttonmap[CLASSIC][6] = WPAD_CLASSIC_BUTTON_A;
	buttonmap[GUITAR][6] = WPAD_GUITAR_HERO_3_BUTTON_GREEN;
	//buttonmap[NUNCHUK][0] = WPAD_NUNCHUK_BUTTON_A;
	
	buttonmap[GCPAD][7] = PAD_BUTTON_B;
	buttonmap[WIIMOTE][7] = WPAD_BUTTON_B;
	buttonmap[CLASSIC][7] = WPAD_CLASSIC_BUTTON_B;
	buttonmap[GUITAR][7] = WPAD_GUITAR_HERO_3_BUTTON_RED;
	//buttonmap[NUNCHUK][0] = WPAD_NUNCHUK_BUTTON_A;
	
	buttonmap[GCPAD][8] = PAD_BUTTON_START;
	buttonmap[WIIMOTE][8] = WPAD_BUTTON_HOME;
	buttonmap[CLASSIC][8] = WPAD_CLASSIC_BUTTON_HOME;
	buttonmap[GUITAR][8] = WPAD_GUITAR_HERO_3_BUTTON_ORANGE;
	//buttonmap[NUNCHUK][0] = WPAD_NUNCHUK_BUTTON_A;
	
	buttonmap[GCPAD][9] = PAD_BUTTON_Y;
	buttonmap[WIIMOTE][9] = WPAD_BUTTON_1;
	buttonmap[CLASSIC][9] = WPAD_CLASSIC_BUTTON_Y;
	buttonmap[GUITAR][9] = WPAD_GUITAR_HERO_3_BUTTON_YELLOW;
	//buttonmap[NUNCHUK][0] = WPAD_NUNCHUK_BUTTON_A;
	
	buttonmap[GCPAD][10] = PAD_BUTTON_X;
	buttonmap[WIIMOTE][10] = WPAD_BUTTON_2;
	buttonmap[CLASSIC][10] = WPAD_CLASSIC_BUTTON_X;
	buttonmap[GUITAR][10] = WPAD_GUITAR_HERO_3_BUTTON_BLUE;
	//buttonmap[NUNCHUK][0] = WPAD_NUNCHUK_BUTTON_A;
}

void __Wpad_PowerCallback(s32 chan)
{
	/* Poweroff console */
	shutdown = 1;
}


s32 Wpad_Init(void)
{
	/* Initialize Wiimote subsystem */
	s32 ret = WPAD_Init();
	PAD_Init();
	makeButtonMap();
	
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

u32 Wpad_GetButtons(void) {
	
	int i = 0;
	u32 buttons = 0;
    WPAD_ScanPads();
    PAD_ScanPads();
		
	// handle shutdown
	if (shutdown)
		Do_Shutdown();
	
    for(; i < MAX_WIIMOTES; i++)
        buttons |= readPad(i, 0);
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

u32 Wpad_WaitButtonsCommon(void)
{
	u32 buttons = 0;

	/* Wait for button pressing */
	buttons = Wpad_WaitButtons();
	extern void Handle_Home(int disable_screenshot);
	if (buttons & WPAD_BUTTON_HOME) {
		Handle_Home(0);
		buttons = Wpad_WaitButtons();
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
			Do_Shutdown();
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
				buttons_held |= Wpad_Held(cnt);
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

