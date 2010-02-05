#ifndef _WPAD_H_
#define _WPAD_H_

#include <wiiuse/wpad.h>
#include <ogc/pad.h>

/* Prototypes */
s32  Wpad_Init(void);
void Wpad_Disconnect(void);
u32  Wpad_GetButtons(void);
u32  Wpad_WaitButtons(void);
u32  Wpad_WaitButtonsRpt(void);
u32  Wpad_WaitButtonsCommon(void);
u32  Wpad_Held(int);
void Wpad_getIR(int n, struct ir_t *ir);

#endif
