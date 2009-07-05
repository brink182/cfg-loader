#ifndef _SYS_H_
#define _SYS_H_

/* Prototypes */
void Sys_Init(void);
void Sys_Reboot(void);
void Sys_Shutdown(void);
void Sys_LoadMenu(void);
void Sys_Exit(void);

s32  Sys_GetCerts(signed_blob **, u32 *);
int ReloadIOS(int subsys, int verbose);
void Block_IOS_Reload();

#endif
