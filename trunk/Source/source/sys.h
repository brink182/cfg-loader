#ifndef _SYS_H_
#define _SYS_H_

/* Prototypes */
void Sys_Init(void);
void Sys_Reboot(void);
void Sys_Shutdown(void);
void Sys_LoadMenu(void);
void Sys_Exit(void);
void Sys_HBC();
void Sys_Channel(u32 channel);

s32  Sys_GetCerts(signed_blob **, u32 *);
int ReloadIOS(int subsys, int verbose);
void Block_IOS_Reload();
void load_bca_data(u8 *discid);
void insert_bca_data();
void print_mload_version();
void load_dip_249();

#define IOS_TYPE_UNK    0
#define IOS_TYPE_WANIN  1
#define IOS_TYPE_HERMES 2
#define IOS_TYPE_KWIIRK 3

int get_ios_type();
int is_ios_type(int type);

#endif
