#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <ogcsys.h>
#include <fat.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sdcard/wiisd_io.h>

#include "wbfs.h"

/* Constants */
//#define SDHC_MOUNT	"sdhc"
#define SDHC_MOUNT	"sd"
#define USB_MOUNT	"usb"

#define FAT_CACHE_SECTORS 256

/* Disc interfaces */
extern const DISC_INTERFACE __io_sdhc;
extern const DISC_INTERFACE __io_usbstorage;

extern s32 wbfsDev;

#define MOUNT_NONE 0
#define MOUNT_SD   1
#define MOUNT_SDHC 2

int sd_mount_mode = MOUNT_NONE;
int usb_mount = 0;

s32 Fat_MountSDHC(void)
{
	s32 ret;

	sd_mount_mode = MOUNT_NONE;

	/* Initialize SDHC interface */
	ret = __io_sdhc.startup();
	if (!ret) {
		//printf("[+] ERROR: SDHC init! (%d)\n", ret); sleep(1);
		goto try_sd;
		//return -1;
	}

	/* Mount device */
	//ret = fatMountSimple(SDHC_MOUNT, &__io_sdhc);
	ret = fatMount(SDHC_MOUNT, &__io_sdhc, 0, FAT_CACHE_SECTORS);
	if (!ret) {
		//printf("[+] ERROR: SDHC/FAT init! (%d)\n", ret); sleep(1);
		return -2;
	}

	sd_mount_mode = MOUNT_SDHC;
	return 0;

	try_sd:
	// SDHC failed, try SD
	ret = __io_wiisd.startup();
	if (!ret) {
		//printf("[+] ERROR: SD init! (%d)\n", ret); sleep(1);
		return -3;
	}
	//ret = fatMountSimple("sd", &__io_wiisd);
	ret = fatMount(SDHC_MOUNT, &__io_wiisd, 0, FAT_CACHE_SECTORS);
	if (!ret) {
		// printf("[+] ERROR: SD/FAT init! (%d)\n", ret); sleep(1);
		return -4;
	}
	//printf("[+] NOTE: SDHC mode not available\n");
	//printf("[+] NOTE: card in standard SD mode\n\n");

	sd_mount_mode = MOUNT_SD;
	return 0;
}

s32 Fat_UnmountSDHC(void)
{
	s32 ret = 1;

	/* Unmount device */
	fatUnmount(SDHC_MOUNT);

	/* Shutdown SDHC interface */
	if (sd_mount_mode == MOUNT_SDHC) {
		// don't shutdown sdhc if we're booting from it
		if (wbfsDev != WBFS_DEVICE_SDHC) {
			ret = __io_sdhc.shutdown();
		}
	} else if (sd_mount_mode == MOUNT_SD) {
		ret = __io_wiisd.shutdown();
	}
	sd_mount_mode = MOUNT_NONE;
	if (!ret)
		return -1;

	return 0;
}

s32 Fat_ReadFile(const char *filepath, void **outbuf)
{
	FILE *fp     = NULL;
	void *buffer = NULL;

	struct stat filestat;
	u32         filelen;

	s32 ret;

	/* Get filestats */
	stat(filepath, &filestat);

	/* Get filesize */
	filelen = filestat.st_size;

	/* Allocate memory */
	buffer = memalign(32, filelen);
	if (!buffer)
		goto err;

	/* Open file */
	fp = fopen(filepath, "rb");
	if (!fp)
		goto err;

	/* Read file */
	ret = fread(buffer, 1, filelen, fp);
	if (ret != filelen)
		goto err;

	/* Set pointer */
	*outbuf = buffer;

	goto out;

err:
	/* Free memory */
	if (buffer)
		free(buffer);

	/* Error code */
	ret = -1;

out:
	/* Close file */
	if (fp)
		fclose(fp);

	return ret;
}

void Fat_print_sd_mode()
{
	printf("FAT32 mount: ");
	if (sd_mount_mode == MOUNT_SD) printf("SD (Standard) ");
	if (sd_mount_mode == MOUNT_SDHC) printf("SD (SDHC) ");
	if (usb_mount) printf("USB-HDD");
	if (sd_mount_mode == MOUNT_NONE && usb_mount == 0) printf("NONE");
	printf("\n");
}


s32 Fat_MountUSB(void)
{
	s32 ret;

	if (usb_mount) return 0;

	/* Initialize USB interface */
	ret = __io_usbstorage.startup();
	if (!ret) {
		//printf("[+] ERROR: USB init! (%d)\n", ret); sleep(1);
		return -1;
	}

	/* Mount device */
	ret = fatMount(USB_MOUNT, &__io_usbstorage, 0, FAT_CACHE_SECTORS);
	if (!ret) {
		//printf("[+] ERROR: USB/FAT init! (%d)\n", ret); sleep(1);
		return -2;
	}

	usb_mount = 1;

	return 0;
}

s32 Fat_UnmountUSB(void)
{
	if (usb_mount == 0) return 0;

	/* Unmount device */
	fatUnmount(USB_MOUNT);

	usb_mount = 0;

	return 0;
}


