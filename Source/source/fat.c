
// Modified by oggzee

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <ogcsys.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sdcard/wiisd_io.h>
#include <string.h>
#include <locale.h>
#include <errno.h>

#include <fat.h>
#include "ntfs.h"

#include "sdhc.h"
#include "wbfs.h"
#include "util.h"
#include "wpad.h"
#include "menu.h"
#include "gettext.h"
#include "cfg.h"
#include "sys.h"


/* Constants */
#define SDHC_MOUNT	"sd"
#define USB_MOUNT	"usb"
#define WBFS_MOUNT	"wbfs"
#define NTFS_MOUNT	"ntfs"

//#define FAT_CACHE 4
//#define FAT_SECTORS 64
//#define FAT_CACHE 64
#define FAT_CACHE 32
#define FAT_SECTORS 64
#define FAT_SECTORS_SD 32
//#define FAT_CACHE_SECTORS 256
#define FAT_CACHE_SECTORS FAT_CACHE, FAT_SECTORS
#define FAT_CACHE_SECTORS_SD FAT_CACHE, FAT_SECTORS_SD

/* Disc interfaces */
extern const DISC_INTERFACE __io_sdhc;
extern const DISC_INTERFACE __io_usbstorage;
// read-only
extern const DISC_INTERFACE __io_sdhc_ro;
extern const DISC_INTERFACE __io_usbstorage_ro;

void _FAT_mem_init();
extern sec_t _FAT_startSector;

extern s32 wbfsDev;

//#define MOUNT_NONE 0
//#define MOUNT_SD   1
//#define MOUNT_SDHC 2

int   fat_sd_mount = 0;
sec_t fat_sd_sec = 0; // u32

int   fat_usb_mount = 0;
sec_t fat_usb_sec = 0;

int   fat_wbfs_mount = 0;
sec_t fat_wbfs_sec = 0;

int   fs_ntfs_mount = 0;
sec_t fs_ntfs_sec = 0;

void fat_Unmount(const char* name);

s32 Fat_MountSDHC(void)
{
	s32 ret;

	if (fat_sd_mount) return 0;
	fat_sd_sec = 0;

	_FAT_mem_init();

	sdhc_mode_sd = 0;

	//if ( (!is_ios_type(IOS_TYPE_WANIN)) ||
	//		 ( is_ios_type(IOS_TYPE_WANIN) && 
	//		  (IOS_GetRevision() == 18 || IOS_GetRevision() > 100) )
	if ( is_ios_type(IOS_TYPE_WANIN) && (IOS_GetRevision() == 18) )
	{
		// sdhc device is broken on ios 249 rev 18
		sdhc_mode_sd = 1;
	}

	/* Initialize SD/SDHC interface */
	retry:
	dbg_printf("SD init\n");
	ret = __io_sdhc.startup();
	if (!ret) {
		dbg_printf("ERROR: SDHC init! (%d)\n", ret); sleep(1);
		if (!sdhc_mode_sd) {
		   sdhc_mode_sd = 1;
		   goto retry;
		}
		return -1;
	}

	dbg_printf("SD fat mount\n");
	/* Mount device */
	if (!sdhc_mode_sd) {
		ret = fatMount(SDHC_MOUNT, &__io_sdhc, 0, FAT_CACHE_SECTORS);
	} else {
		ret = fatMount(SDHC_MOUNT, &__io_sdhc, 0, FAT_CACHE_SECTORS_SD);
	}
	if (!ret) {
		//printf_x("ERROR: SDHC/FAT init! (%d)\n", ret); sleep(1);
		return -2;
	}

	fat_sd_mount = 1;
	fat_sd_sec = _FAT_startSector;
	return 0;

#if 0
	try_sd:
	// SDHC failed, try SD
	ret = __io_wiisd.startup();
	if (!ret) {
		//printf_x("ERROR: SD init! (%d)\n", ret); sleep(1);
		return -3;
	}
	sdhc_mode_sd = 1;
	//ret = fatMountSimple("sd", &__io_wiisd);
	ret = fatMount(SDHC_MOUNT, &__io_wiisd, 0, FAT_CACHE_SECTORS_SD);
	//ret = fatMount(SDHC_MOUNT, &__io_wiisd, 0, FAT_CACHE, FAT_SECTORS);
	if (!ret) {
		// printf_x("ERROR: SD/FAT init! (%d)\n", ret); sleep(1);
		return -4;
	}
	//printf_x("NOTE: SDHC mode not available\n");
	//printf_x("NOTE: card in standard SD mode\n\n");

	fat_sd_mount = 1;
	fat_sd_sec = _FAT_startSector;

	return 0;
#endif
}

s32 Fat_UnmountSDHC(void)
{
	s32 ret = 1;
	if (!fat_sd_mount) return 0;

	/* Unmount device */
	fat_Unmount(SDHC_MOUNT);

	/* Shutdown SDHC interface */
	if (sdhc_mode_sd == 0) {
		// don't shutdown sdhc if we're booting from it
		if (wbfsDev != WBFS_DEVICE_SDHC) {
			//ret = __io_sdhc.shutdown(); // this is NOP
			SDHC_Close();
		}
	} else {
		ret = __io_wiisd.shutdown();
	}
	fat_sd_mount = 0;
	fat_sd_sec = 0;
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
	ret = stat(filepath, &filestat);
	if (ret != 0) {
		//printf("File not found %s %d\n", filepath, (int)filestat.st_size); sleep(3);
		return -1;
	}

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
	printf(gt("FAT32 mount: "));
	if (fat_sd_mount) printf("%s ", sdhc_mode_sd ? "sd" : "SD");
	if (fat_usb_mount) printf("USB-HDD ");
	if (fat_wbfs_mount) printf("WBFS");
	if (!fat_sd_mount && !fat_usb_mount && !fat_wbfs_mount) printf("NONE");
	printf("\n");
}


s32 Fat_MountUSB(void)
{
	s32 ret;

	if (fat_usb_mount) return 0;
	_FAT_mem_init();

	/* Initialize USB interface */
	ret = __io_usbstorage.startup();
	if (!ret) {
		//printf_x("ERROR: USB init! (%d)\n", ret); sleep(1);
		return -1;
	}

	/* Mount device */
	ret = fatMount(USB_MOUNT, &__io_usbstorage, 0, FAT_CACHE_SECTORS);
	//ret = fatMount(USB_MOUNT, &__io_usbstorage, 0, FAT_CACHE, FAT_SECTORS);
	if (!ret) {
		//printf_x("ERROR: USB/FAT init! (%d)\n", ret); sleep(1);
		return -2;
	}

	fat_usb_mount = 1;
	fat_usb_sec = _FAT_startSector;

	return 0;
}

s32 Fat_UnmountUSB(void)
{
	if (fat_usb_mount == 0) return 0;

	/* Unmount device */
	fat_Unmount(USB_MOUNT);

	fat_usb_mount = 0;
	fat_usb_sec = 0;

	return 0;
}

s32 Fat_MountWBFS(u32 sector)
{
	s32 ret;

	if (fat_wbfs_mount) return 0;
	_FAT_mem_init();

	if (wbfsDev == WBFS_DEVICE_USB) {
		/* Initialize WBFS interface */
		ret = __io_usbstorage.startup();
		if (!ret) {
			printf_x(gt("ERROR: USB init! (%d)"), ret); printf("\n"); sleep(1);
			return -1;
		}
		/* Mount device */
		ret = fatMount(WBFS_MOUNT, &__io_usbstorage, sector, FAT_CACHE_SECTORS);
		if (!ret) {
			printf_x(gt("ERROR: USB/FAT mount wbfs! (%d)"), ret); printf("\n"); sleep(1);
			return -2;
		}
	} else if (wbfsDev == WBFS_DEVICE_SDHC) {
		if (fat_sd_mount == 0) {
			ret = 0;
		} else if (sdhc_mode_sd == 0) {
			ret = fatMount(SDHC_MOUNT, &__io_sdhc, 0, FAT_CACHE_SECTORS);
		} else {
			ret = fatMount(SDHC_MOUNT, &__io_sdhc, 0, FAT_CACHE_SECTORS_SD);
			//ret = fatMount(SDHC_MOUNT, &__io_wiisd, 0, FAT_CACHE_SECTORS_SD);
		}
		if (!ret) {
			printf_x(gt("ERROR: SD/FAT mount wbfs! (%d)"), ret); printf("\n"); sleep(1);
			return -5;
		}
	}

	fat_wbfs_mount = 1;
	fat_wbfs_sec = _FAT_startSector;
	if (sector && fat_wbfs_sec != sector) {
		printf(gt("ERROR: WBFS FAT mount sector %x not %x"),
				fat_wbfs_sec, sector);
		sleep(2);
		Wpad_WaitButtons();
	}

	return 0;
}

s32 Fat_UnmountWBFS(void)
{
	if (fat_wbfs_mount == 0) return 0;

	/* Unmount device */
	fat_Unmount(WBFS_MOUNT);

	fat_wbfs_mount = 0;
	fat_wbfs_sec = 0;

	return 0;
}

void ntfsInit();

s32 MountNTFS(u32 sector)
{
	s32 ret;

	if (fs_ntfs_mount) return 0;
	//printf("mounting NTFS\n");
	//Wpad_WaitButtons();
	_FAT_mem_init();
	ntfsInit();
	// ntfsInit resets locale settings
	// which breaks unicode in console
	// so we change it back to C-UTF-8
	setlocale(LC_CTYPE, "C-UTF-8");
	setlocale(LC_MESSAGES, "C-UTF-8");

	if (wbfsDev == WBFS_DEVICE_USB) {
		/* Initialize WBFS interface */
		ret = __io_usbstorage.startup();
		if (!ret) {
			printf_x(gt("ERROR: USB init! (%d)"), ret); printf("\n"); sleep(1);
			return -1;
		}
		/* Mount device */
		ret = ntfsMount(NTFS_MOUNT, &__io_usbstorage_ro, sector, FAT_CACHE_SECTORS, NTFS_DEFAULT);
		if (!ret) {
			printf_x(gt("ERROR: USB mount NTFS! (%d)"), ret);
			printf("(%d)\n", errno);
			sleep(2);
			return -2;
		}
	} else if (wbfsDev == WBFS_DEVICE_SDHC) {
		if (sdhc_mode_sd == 0) {
			ret = ntfsMount(NTFS_MOUNT, &__io_sdhc_ro, 0, FAT_CACHE_SECTORS, NTFS_DEFAULT);
		} else {
			ret = ntfsMount(NTFS_MOUNT, &__io_sdhc_ro, 0, FAT_CACHE_SECTORS_SD, NTFS_DEFAULT);
		}
		if (!ret) {
			printf_x(gt("ERROR: SD mount NTFS! (%d)"), ret); printf("\n"); sleep(1);
			return -5;
		}
	}

	fs_ntfs_mount = 1;
	fs_ntfs_sec = sector; //_FAT_startSector;
	/*if (sector && fat_wbfs_sec != sector) {
		printf("ERROR: WBFS FAT mount sector %x not %x\n",
				fat_wbfs_sec, sector);
		sleep(2);
		Wpad_WaitButtons();
	}*/

	return 0;
}

s32 UnmountNTFS(void)
{
	//printf("Unmount NTFS\n");
	if (fs_ntfs_mount == 0) return 0;

	/* Unmount device */
	ntfsUnmount(NTFS_MOUNT, true);

	fs_ntfs_mount = 0;
	fs_ntfs_sec = 0;

	return 0;
}

void Fat_UnmountAll()
{
	Fat_UnmountSDHC();
	Fat_UnmountUSB();
	Fat_UnmountWBFS();
	UnmountNTFS();
}

// fat cache alloc

#if 1

static void *fat_pool = NULL;
static size_t fat_size;
#define FAT_SLOTS (FAT_CACHE * 3)
#define FAT_SLOT_SIZE (512 * FAT_SECTORS)
#define FAT_SLOT_SIZE_MIN (512 * FAT_SECTORS_SD)
static int fat_alloc[FAT_SLOTS];

void _FAT_mem_init()
{
	if (fat_pool) return;
	fat_size = FAT_SLOTS * FAT_SLOT_SIZE;
	fat_pool = LARGE_memalign(32, fat_size);
}

void* _FAT_mem_allocate(size_t size)
{
	return malloc(size);
}

void* _FAT_mem_align(size_t size)
{
	if (size < FAT_SLOT_SIZE_MIN || size > FAT_SLOT_SIZE) goto fallback;
	if (fat_pool == NULL) goto fallback;
	int i;
	for (i=0; i<FAT_SLOTS; i++) {
		if (fat_alloc[i] == 0) {
			void *ptr = fat_pool + i * FAT_SLOT_SIZE;
			fat_alloc[i] = 1;
			return ptr;
		}	
	}
	fallback:
	return memalign (32, size);		
}

void _FAT_mem_free(void *mem)
{
	if (fat_pool == NULL || mem < fat_pool || mem >= fat_pool + fat_size) {
		free(mem);
		return;
	}
	int i;
	for (i=0; i<FAT_SLOTS; i++) {
		if (fat_alloc[i]) {
			void *ptr = fat_pool + i * FAT_SLOT_SIZE;
			if (mem == ptr) {
				fat_alloc[i] = 0;
				return;
			}
		}	
	}
	// FATAL
	printf("\n");
	printf(gt("FAT: ALLOC ERROR %p %p %p"), mem, fat_pool, fat_pool + fat_size);
	printf("\n");
	sleep(5);
	exit(1);
}

#else

void _FAT_mem_init()
{
}

void* _FAT_mem_allocate (size_t size) {
	return malloc (size);
}

void* _FAT_mem_align (size_t size) {
	return memalign (32, size);
}

void _FAT_mem_free (void* mem) {
	free (mem);
}

#endif


void* ntfs_alloc (size_t size)
{
	return _FAT_mem_allocate(size);
}

void* ntfs_align (size_t size)
{
	return _FAT_mem_align(size);
}

void ntfs_free (void* mem)
{
	_FAT_mem_free(mem);
}

// fix for newlib/libfat unmount without :

void fat_Unmount (const char* name)
{
	char name2[16];
	strcpy(name2, name);
	strcat(name2, ":");
	fatUnmount(name2);
}

#if 0
#include <sys/iosupport.h>
//extern int FindDevice(const char* name);
//extern const devoptab_t* GetDeviceOpTab (const char *name);
void check_dev(char *name)
{
	printf("DEV: %s %d %p\n", name, FindDevice(name), GetDeviceOpTab(name));
}
	check_dev("sd");
	check_dev("sd:");
	check_dev("usb");
	check_dev("usb:");
#endif

