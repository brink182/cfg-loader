#ifndef _WBFS_H_
#define _WBFS_H_

#include "libwbfs/libwbfs.h"
#include "util.h"

/* Device list */
enum {
	WBFS_DEVICE_USB = 1,	/* USB device */
	WBFS_DEVICE_SDHC	/* SDHC device */
};

/* Macros */
#define WBFS_MIN_DEVICE		1
#define WBFS_MAX_DEVICE		2

#define PART_FS_WBFS 0
#define PART_FS_FAT  1
#define PART_FS_NTFS 2

extern s32 wbfsDev;
extern int wbfs_part_fs;
extern u32 wbfs_part_idx;
extern u32 wbfs_part_lba;
extern char wbfs_fs_drive[16];

/* Prototypes */
s32 WBFS_Init(u32, u32);
s32 WBFS_Open(void);
s32 WBFS_Format(u32, u32);
s32 WBFS_GetCount(u32 *);
s32 WBFS_GetHeaders(void *, u32, u32);
s32 WBFS_CheckGame(u8 *);
s32 WBFS_AddGame(void);
s32 WBFS_RemoveGame(u8 *);
s32 WBFS_GameSize(u8 *, f32 *);
s32 WBFS_GameSize2(u8 *discid, u64 *comp_size, u64 *real_size);
s32 WBFS_DVD_Size(u64 *comp_size, u64 *real_size);
s32 WBFS_DiskSpace(f32 *, f32 *);

s32 WBFS_OpenPart(u32 part_fs, u32 part_idx, u32 part_lba, u32 part_size, char *partition);
s32 WBFS_OpenNamed(char *partition);
s32 WBFS_OpenLBA(u32 lba, u32 size);
wbfs_disc_t* WBFS_OpenDisc(u8 *discid);
void WBFS_CloseDisc(wbfs_disc_t *disc);
bool WBFS_Close();
bool WBFS_Mounted();
bool WBFS_Selected();

#define DOL_LIST_MAX 64
typedef struct DOL_LIST
{
	char name[DOL_LIST_MAX][64];
	int num;
} DOL_LIST;

int WBFS_GetDolList(u8 *discid, DOL_LIST *list);
int WBFS_BannerSound(u8 *discid, SoundInfo *snd);

#endif
