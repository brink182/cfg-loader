#ifndef _SYS_H_
#define _SYS_H_

/* Prototypes */
void Sys_Init(void);
void Sys_Reboot(void);
void Sys_Shutdown(void);
void Sys_LoadMenu(void);
void Sys_Exit(void);
void Sys_HBC();

s32  Sys_GetCerts(signed_blob **, u32 *);
int ReloadIOS(int subsys, int verbose);
void Block_IOS_Reload();
void load_bca_data(u8 *discid);
void insert_bca_data();

#endif
