
// FAT support, banner sounds and alt.dol by oggzee
// Banner title for playlog by Clipper

#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <ogcsys.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <ctype.h>
#include <sdcard/wiisd_io.h>

#include "libwbfs/libwbfs.h"
#include "sdhc.h"
#include "usbstorage.h"
#include "utils.h"
#include "video.h"
#include "wbfs.h"
#include "wdvd.h"
#include "subsystem.h"
#include "splits.h"
#include "fat.h"
#include "partition.h"
#include "cfg.h"
#include "wbfs_fat.h"
#include "util.h"
#include "gettext.h"

/* Constants */
#define MAX_NB_SECTORS	32

/* WBFS device */
s32 wbfsDev = WBFS_MIN_DEVICE;

// partition
int wbfs_part_fs  = PART_FS_WBFS;
u32 wbfs_part_idx = 0;
u32 wbfs_part_lba = 0;

/* WBFS HDD */
wbfs_t *hdd = NULL;

/* WBFS callbacks */
static rw_sector_callback_t readCallback  = NULL;
static rw_sector_callback_t writeCallback = NULL;

/* Variables */
static u32 nb_sectors, sector_size;

void __WBFS_Spinner(s32 x, s32 max)
{
	static time_t start;
	static u32 expected;

	f32 percent, size;
	u32 d, h, m, s;

	/* First time */
	if (!x) {
		start    = time(0);
		expected = 300;
	}

	/* Elapsed time */
	d = time(0) - start;

	if (x != max) {
		/* Expected time */
		if (d && x)
			expected = (expected * 3 + d * max / x) / 4;

		/* Remaining time */
		d = (expected > d) ? (expected - d) : 1;
	}

	/* Calculate time values */
	h =  d / 3600;
	m = (d / 60) % 60;
	s =  d % 60;

	/* Calculate percentage/size */
	percent = (x * 100.0) / max;
	size    = (hdd->wii_sec_sz / GB_SIZE) * max;

	Con_ClearLine();

	/* Show progress */
	if (x != max) {
		printf_(gt("%.2f%% of %.2fGB (%c) ETA: %d:%02d:%02d"),
				percent, size, "/-\\|"[(x / 10) % 4], h, m, s);
		printf("\r");
		fflush(stdout);
	} else {
		printf_(gt("%.2fGB copied in %d:%02d:%02d"), size, h, m, s);
		printf("  \n");
	}

	__console_flush(1);
}

s32 __WBFS_ReadDVD(void *fp, u32 lba, u32 len, void *iobuf)
{
	void *buffer = NULL;

	u64 offset;
	u32 mod, size;
	s32 ret;

	/* Calculate offset */
	offset = ((u64)lba) << 2;

	/* Calcualte sizes */
	mod  = len % 32;
	size = len - mod;

	/* Read aligned data */
	if (size) {
		ret = WDVD_UnencryptedRead(iobuf, size, offset);
		if (ret < 0)
			goto out;
	}

	/* Read non-aligned data */
	if (mod) {
		/* Allocate memory */
		buffer = memalign(32, 0x20);
		if (!buffer)
			return -1;

		/* Read data */
		ret = WDVD_UnencryptedRead(buffer, 0x20, offset + size);
		if (ret < 0)
			goto out;

		/* Copy data */
		memcpy(iobuf + size, buffer, mod);
	}

	/* Success */
	ret = 0;

out:
	/* Free memory */
	if (buffer)
		free(buffer);

	return ret;
}

s32 __WBFS_ReadUSB(void *fp, u32 lba, u32 count, void *iobuf)
{
	u32 cnt = 0;
	s32 ret;

	/* Do reads */
	while (cnt < count) {
		void *ptr     = ((u8 *)iobuf) + (cnt * sector_size);
		u32   sectors = (count - cnt);

		/* Read sectors is too big */
		if (sectors > MAX_NB_SECTORS)
			sectors = MAX_NB_SECTORS;

		/* USB read */
		ret = USBStorage_ReadSectors(lba + cnt, sectors, ptr);
		if (ret < 0)
			return ret;

		/* Increment counter */
		cnt += sectors;
	}

	return 0;
}

s32 __WBFS_WriteUSB(void *fp, u32 lba, u32 count, void *iobuf)
{
	u32 cnt = 0;
	s32 ret;

	/* Do writes */
	while (cnt < count) {
		void *ptr     = ((u8 *)iobuf) + (cnt * sector_size);
		u32   sectors = (count - cnt);

		/* Write sectors is too big */
		if (sectors > MAX_NB_SECTORS)
			sectors = MAX_NB_SECTORS;

		/* USB write */
		ret = USBStorage_WriteSectors(lba + cnt, sectors, ptr);
		if (ret < 0)
			return ret;

		/* Increment counter */
		cnt += sectors;
	}

	return 0;
}

s32 __WBFS_ReadSDHC(void *fp, u32 lba, u32 count, void *iobuf)
{
	u32 cnt = 0;
	s32 ret;

	/* Do reads */
	while (cnt < count) {
		void *ptr     = ((u8 *)iobuf) + (cnt * sector_size);
		u32   sectors = (count - cnt);

		/* Read sectors is too big */
		if (sectors > MAX_NB_SECTORS)
			sectors = MAX_NB_SECTORS;

		/* SDHC read */
		ret = SDHC_ReadSectors(lba + cnt, sectors, ptr);
		if (!ret)
			return -1;

		/* Increment counter */
		cnt += sectors;
	}

	return 0;
}

s32 __WBFS_WriteSDHC(void *fp, u32 lba, u32 count, void *iobuf)
{
	u32 cnt = 0;
	s32 ret;

	/* Do writes */
	while (cnt < count) {
		void *ptr     = ((u8 *)iobuf) + (cnt * sector_size);
		u32   sectors = (count - cnt);

		/* Write sectors is too big */
		if (sectors > MAX_NB_SECTORS)
			sectors = MAX_NB_SECTORS;

		/* SDHC write */
		ret = SDHC_WriteSectors(lba + cnt, sectors, ptr);
		if (!ret)
			return -1;

		/* Increment counter */
		cnt += sectors;
	}

	return 0;
}


s32 WBFS_Init(u32 device, u32 timeout)
{
	u32 cnt;
	s32 ret = -1;

	/* Wrong timeout */
	if (!timeout)
		return -1;

	/* Try to mount device */
	for (cnt = 0; cnt < timeout; cnt++) {
		switch (device) {
		case WBFS_DEVICE_USB: {
			/* Initialize USB storage */
			ret = USBStorage_Init();

			if (ret >= 0) {
				/* Setup callbacks */
				readCallback  = __WBFS_ReadUSB;
				writeCallback = __WBFS_WriteUSB;

				/* Device info */
				nb_sectors = USBStorage_GetCapacity(&sector_size);

				goto out;
			}
			break;
		}

		case WBFS_DEVICE_SDHC: {
			/* Initialize SDHC */
			ret = SDHC_Init();
			// returns true=ok false=error 
			if (!ret && !sdhc_mode_sd) {
				// try normal SD
				sdhc_mode_sd = 1;
				ret = SDHC_Init();
			}
			if (ret) {
				/* Setup callbacks */
				readCallback  = __WBFS_ReadSDHC;
				writeCallback = __WBFS_WriteSDHC;

				/* Device info */
				nb_sectors  = 0;
				sector_size = SDHC_SECTOR_SIZE;

				goto out;
			}

			ret = -1;
			break;
		}

		default:
			return -1;
		}

		Gui_Console_Enable();
		printf("%d ", cnt + 1);

		/* Sleep 1 second */
		sleep(1);
	}

out:
	printf_("%s\n", ret < 0 ? gt("ERROR!") : gt("OK!"));
	return ret;
}

bool WBFS_Close()
{
	/* Close hard disk */
	if (hdd) {
		wbfs_close(hdd);
		hdd = NULL;
	}
	Fat_UnmountWBFS();
	UnmountNTFS();
	wbfs_part_fs = 0;
	wbfs_part_idx = 0;
	wbfs_part_lba = 0;
	strcpy(wbfs_fs_drive, "");

	return 0;
}

bool WBFS_Mounted()
{
	return (hdd != NULL);
}

bool WBFS_Selected()
{
	if (wbfs_part_fs && wbfs_part_lba && *wbfs_fs_drive) return true;
	return WBFS_Mounted();
}

s32 WBFS_Open(void)
{
	/* Close hard disk */
	if (hdd)
		wbfs_close(hdd);
	
	/* Open hard disk */
	wbfs_part_fs = 0;
	wbfs_part_idx = 0;
	wbfs_part_lba = 0;
	hdd = wbfs_open_hd(readCallback, writeCallback, NULL, sector_size, nb_sectors, 0);
	if (!hdd)
		return -1;
	wbfs_part_idx = 1;

	return 0;
}

s32 WBFS_OpenPart(u32 part_fs, u32 part_idx, u32 part_lba, u32 part_size, char *partition)
{
	// close
	WBFS_Close();

	if (part_fs == PART_FS_FAT) {
		//if (wbfsDev != WBFS_DEVICE_USB) return -1;
		if (wbfsDev == WBFS_DEVICE_USB && part_lba == fat_usb_sec) {
			strcpy(wbfs_fs_drive, "usb:");
		} else if (wbfsDev == WBFS_DEVICE_SDHC && part_lba == fat_sd_sec) {
			strcpy(wbfs_fs_drive, "sd:");
		} else {
			if (Fat_MountWBFS(part_lba)) return -1;
			strcpy(wbfs_fs_drive, "wbfs:");
		}
	} else if (part_fs == PART_FS_NTFS) {
		if (MountNTFS(part_lba)) return -1;
		strcpy(wbfs_fs_drive, "ntfs:");
	} else {
		if (WBFS_OpenLBA(part_lba, part_size)) return -1;
	}

	// success
	wbfs_part_fs  = part_fs;
	wbfs_part_idx = part_idx;
	wbfs_part_lba = part_lba;
	char *fs = "WBFS";
	if (wbfs_part_fs == PART_FS_FAT) fs = "FAT";
	if (wbfs_part_fs == PART_FS_NTFS) fs = "NTFS";
	sprintf(partition, "%s%d", fs, wbfs_part_idx);
	return 0;
}

s32 WBFS_OpenNamed(char *partition)
{
	int i;
	u32 part_fs  = PART_FS_WBFS;
	u32 part_idx = 0;
	u32 part_lba = 0;
	s32 ret = 0;
	PartList plist;

	// close
	WBFS_Close();

	// parse partition option
	if (strncasecmp(partition, "WBFS", 4) == 0) {
		i = atoi(partition+4);
		if (i < 1 || i > 4) goto err;
		part_fs  = PART_FS_WBFS;
		part_idx = i;
	} else if (strncasecmp(partition, "FAT", 3) == 0) {
		i = atoi(partition+3);
		if (i < 1 || i > 9) goto err;
		part_fs  = PART_FS_FAT;
		part_idx = i;
	} else if (strncasecmp(partition, "NTFS", 4) == 0) {
		i = atoi(partition+4);
		if (i < 1 || i > 9) goto err;
		part_fs  = PART_FS_NTFS;
		part_idx = i;
	} else {
		goto err;
	}

	// Get partition entries
	ret = Partition_GetList(wbfsDev, &plist);
	if (ret || plist.num == 0) return -1;

	if (part_fs == PART_FS_WBFS) {
		if (part_idx > plist.wbfs_n) goto err;
		for (i=0; i<plist.num; i++) {
			if (plist.pinfo[i].wbfs_i == part_idx) break;
		}
	} else if (part_fs == PART_FS_FAT) {
		if (part_idx > plist.fat_n) goto err;
		for (i=0; i<plist.num; i++) {
			if (plist.pinfo[i].fat_i == part_idx) break;
		}
	} else if (part_fs == PART_FS_NTFS) {
		if (part_idx > plist.ntfs_n) goto err;
		for (i=0; i<plist.num; i++) {
			if (plist.pinfo[i].ntfs_i == part_idx) break;
		}
	}
	if (i >= plist.num) goto err;
	// set partition lba sector
	part_lba = plist.pentry[i].sector;

	if (WBFS_OpenPart(part_fs, part_idx, part_lba, plist.pentry[i].size, partition)) {
		goto err;
	}
	// success
	return 0;

err:
	Gui_Console_Enable();
	printf(gt("Invalid partition: '%s'"), partition);
	printf("\n");
	__console_flush(0);
	sleep(2);
	return -1;
}


s32 WBFS_OpenLBA(u32 lba, u32 size)
{
	wbfs_t *part = NULL;

	/* Open partition */
	part = wbfs_open_partition(readCallback, writeCallback, NULL, sector_size, size, lba, 0);
	if (!part) return -1;

	/* Close current hard disk */
	if (hdd) wbfs_close(hdd);
	hdd = part;

	return 0;
}

s32 WBFS_Format(u32 lba, u32 size)
{
	wbfs_t *partition = NULL;

	/* Reset partition */
	partition = wbfs_open_partition(readCallback, writeCallback, NULL, sector_size, size, lba, 1);
	if (!partition)
		return -1;

	/* Free memory */
	wbfs_close(partition);

	return 0;
}

s32 WBFS_GetCount(u32 *count)
{
	if (wbfs_part_fs) return WBFS_FAT_GetCount(count);

	/* No device open */
	if (!hdd)
		return -1;

	/* Get list length */
	*count = wbfs_count_discs(hdd);

	return 0;
}

s32 WBFS_GetHeaders(void *outbuf, u32 cnt, u32 len)
{
	if (wbfs_part_fs) return WBFS_FAT_GetHeaders(outbuf, cnt, len);

	u32 idx, size;
	s32 ret;

	/* No device open */
	if (!hdd)
		return -1;

	for (idx = 0; idx < cnt; idx++) {
		u8 *ptr = ((u8 *)outbuf) + (idx * len);

		/* Get header */
		ret = wbfs_get_disc_info(hdd, idx, ptr, len, &size);
		if (ret != 0)
			return ret;
	}

	return 0;
}

s32 WBFS_CheckGame(u8 *discid)
{
	wbfs_disc_t *disc = NULL;

	/* Try to open game disc */
	disc = WBFS_OpenDisc(discid);
	if (disc) {
		/* Close disc */
		WBFS_CloseDisc(disc);
		return 1;
	}

	return 0;
}

s32 WBFS_AddGame(void)
{
	if (wbfs_part_fs) return WBFS_FAT_AddGame();
	s32 ret;

	/* No device open */
	if (!hdd)
		return -1;

	/* Add game to device */
	partition_selector_t part_sel = ALL_PARTITIONS;
	int copy_1_1 = 0;
	switch (CFG.install_partitions) {
		case CFG_INSTALL_GAME:
			part_sel = ONLY_GAME_PARTITION;
			break;
		case CFG_INSTALL_ALL:
			part_sel = ALL_PARTITIONS;
			break;
		case CFG_INSTALL_1_1:
			part_sel = ALL_PARTITIONS;
			copy_1_1 = 1;
			break;
	}
	ret = wbfs_add_disc(hdd, __WBFS_ReadDVD, NULL, __WBFS_Spinner, part_sel, copy_1_1);
	if (ret < 0)
		return ret;

	return 0;
}

s32 WBFS_RemoveGame(u8 *discid)
{
	if (wbfs_part_fs) return WBFS_FAT_RemoveGame(discid);
	s32 ret;

	/* No device open */
	if (!hdd)
		return -1;

	/* Remove game from device */
	ret = wbfs_rm_disc(hdd, discid);
	if (ret < 0)
		return ret;

	return 0;
}

s32 WBFS_GameSize(u8 *discid, f32 *size)
{
	wbfs_disc_t *disc = NULL;

	u32 sectors;

	/* Open disc */
	disc = WBFS_OpenDisc(discid);
	if (!disc)
		return -2;

	/* Get game size in sectors */
	sectors = wbfs_disc_sector_used(disc, NULL);

	/* Copy value */
	*size = (disc->p->wbfs_sec_sz / GB_SIZE) * sectors;

	/* Close disc */
	WBFS_CloseDisc(disc);

	return 0;
}

s32 WBFS_GameSize2(u8 *discid, u64 *comp_size, u64 *real_size)
{
	wbfs_disc_t *disc = NULL;

	u32 sectors, real_sec;

	/* Open disc */
	disc = WBFS_OpenDisc(discid);
	if (!disc)
		return -2;

	/* Get game size in sectors */
	sectors = wbfs_disc_sector_used(disc, &real_sec);

	/* Copy value */
	*comp_size = ((u64)disc->p->wbfs_sec_sz) * sectors;
	*real_size = ((u64)disc->p->wbfs_sec_sz) * real_sec;

	/* Close disc */
	WBFS_CloseDisc(disc);

	return 0;
}

s32 WBFS_DVD_Size(u64 *comp_size, u64 *real_size)
{
	if (wbfs_part_fs) return WBFS_FAT_DVD_Size(comp_size, real_size);
	s32 ret;
	u32 comp_sec = 0, last_sec = 0;

	/* No device open */
	if (!hdd)
		return -1;

	/* Add game to device */
	partition_selector_t part_sel;
	if (CFG.install_partitions) {
		part_sel = ALL_PARTITIONS;
	} else {
		part_sel = ONLY_GAME_PARTITION;
	}
	ret = wbfs_size_disc(hdd, __WBFS_ReadDVD, NULL, part_sel, &comp_sec, &last_sec);
	if (ret < 0)
		return ret;

	*comp_size = ((u64)hdd->wii_sec_sz) * comp_sec;
	*real_size = ((u64)hdd->wii_sec_sz) * (last_sec+1);

	return 0;
}


s32 WBFS_DiskSpace(f32 *used, f32 *free)
{
	if (wbfs_part_fs) return WBFS_FAT_DiskSpace(used, free);
	f32 ssize;
	u32 cnt;

	/* No device open */
	if (!hdd)
		return -1;

	/* Count used blocks */
	cnt = wbfs_count_usedblocks(hdd);

	/* Sector size in GB */
	ssize = hdd->wbfs_sec_sz / GB_SIZE;

	/* Copy values */
	*free = ssize * cnt;
	*used = ssize * (hdd->n_wbfs_sec - cnt);

	return 0;
}

wbfs_disc_t* WBFS_OpenDisc(u8 *discid)
{
	if (wbfs_part_fs) return WBFS_FAT_OpenDisc(discid);

	/* No device open */
	if (!hdd)
		return NULL;

	/* Open disc */
	return wbfs_open_disc(hdd, discid);
}

void WBFS_CloseDisc(wbfs_disc_t *disc)
{
	if (wbfs_part_fs) {
		WBFS_FAT_CloseDisc(disc);
		return;
	}

	/* No device open */
	if (!hdd || !disc)
		return;

	/* Close disc */
	wbfs_close_disc(disc);
}

typedef struct {
	u8 filetype;
	char name_offset[3];
	u32 fileoffset;
	u32 filelen;
} __attribute__((packed)) FST_ENTRY;


char *fstfilename2(FST_ENTRY *fst, u32 index)
{
	u32 count = _be32((u8*)&fst[0].filelen);
	u32 stringoffset;
	if (index < count)
	{
		//stringoffset = *(u32 *)&(fst[index]) % (256*256*256);
		stringoffset = _be32((u8*)&(fst[index])) % (256*256*256);
		return (char *)((u32)fst + count*12 + stringoffset);
	} else
	{
		return NULL;
	}
}

int WBFS_GetDolList(u8 *discid, DOL_LIST *list)
{
	FST_ENTRY *fst = NULL;
	int fst_size;

	list->num = 0;

	wbfs_disc_t* d = WBFS_OpenDisc(discid);
	if (!d) return -1;
	fst_size = wbfs_extract_file(d, "", (void*)&fst);
	WBFS_CloseDisc(d);
	if (!fst) return -1;

	u32 count = _be32((u8*)&fst[0].filelen);
	u32 i;

	for (i=1;i<count;i++) {
		char * fname = fstfilename2(fst, i);
		int len = strlen(fname);
		if (len > 4 && stricmp(fname+len-4, ".dol") == 0) {
			if (list->num >= DOL_LIST_MAX) break;
			STRCOPY(list->name[list->num], fname);
			list->num++;
		}
	}

	free(fst);
	return 0;
}

int WBFS_Banner(u8 *discid, SoundInfo *snd, u8 *title, u8 getSound, u8 getTitle)
{
	void *banner = NULL;
	int size;

	if (!getSound && !getTitle) return 0;

	snd->dsp_data = NULL;
	wbfs_disc_t* d = WBFS_OpenDisc(discid);
	if (!d) return -1;
	size = wbfs_extract_file(d, "opening.bnr", &banner);
	WBFS_CloseDisc(d);
	if (!banner || size <= 0) return -1;

	//printf("\nopening.bnr: %d\n", size);
	if (getTitle) 
	{
		s32 lang = getTitle - 2;
		if (lang < 0)
			lang = CFG_read_active_game_setting(discid).language - 1;
		if (lang < 0)
			lang = CONF_GetLanguage();
		parse_banner_title(banner, title, lang);
		// if title is empty revert to english
		char z2[2] = {0,0}; // utf16: 2 bytes
		if (getTitle == 1 && memcmp(title, z2, 2) == 0) {
			parse_banner_title(banner, title, CONF_LANG_ENGLISH);
			// if still empty, find first valid.
			if (memcmp(title, z2, 2) == 0) {
				for (lang=0; lang<10; lang++) {
					parse_banner_title(banner, title, lang);
					if (memcmp(title, z2, 2) != 0) break;
				}
			}
			if (memcmp(title, z2, 2) == 0) {
				// final check, if still empty, use english
				// in case there is some text after the zeroes (unlikely)
				parse_banner_title(banner, title, CONF_LANG_ENGLISH);
			}
		}

	}
	if (getSound) parse_banner_snd(banner, snd);
	SAFE_FREE(banner);
	return 0;
}


