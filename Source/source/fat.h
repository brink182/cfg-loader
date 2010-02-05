
// Modified by oggzee

#ifndef _FAT_H_
#define _FAT_H_

/* Prototypes */
s32 Fat_MountSDHC(void);
s32 Fat_UnmountSDHC(void);
s32 Fat_ReadFile(const char *, void **);
s32 Fat_MountUSB(void);
s32 Fat_UnmountUSB(void);
s32 Fat_MountWBFS(u32 sector);
s32 Fat_UnmountWBFS(void);
s32 MountNTFS(u32 sector);
s32 UnmountNTFS(void);

void Fat_UnmountAll();
void Fat_print_sd_mode();

extern int   fat_sd_mount;
extern sec_t fat_sd_sec;
extern int   fat_usb_mount;
extern sec_t fat_usb_sec;
extern int   fat_wbfs_mount;
extern sec_t fat_wbfs_sec;
extern int   fs_ntfs_mount;
extern sec_t fs_ntfs_sec;

#endif
