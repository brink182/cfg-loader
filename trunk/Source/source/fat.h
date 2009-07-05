#ifndef _FAT_H_
#define _FAT_H_

/* Prototypes */
s32 Fat_MountSDHC(void);
s32 Fat_UnmountSDHC(void);
s32 Fat_ReadFile(const char *, void **);
s32 Fat_MountUSB(void);
s32 Fat_UnmountUSB(void);
void Fat_print_sd_mode();

#endif
