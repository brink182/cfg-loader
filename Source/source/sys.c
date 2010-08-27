#include <stdio.h>
#include <ogcsys.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>

#include "sys.h"
#include "subsystem.h"
#include "cfg.h"
#include "fat.h"
#include "menu.h"
#include "gettext.h"
#include "sha1.h"


#define TITLE_ID(x,y)       (((u64)(x) << 32) | (y))
#define TITLE_HIGH(x)       ((u32)((x) >> 32))
#define TITLE_LOW(x)		((u32)(x))


/* Constants */
#define CERTS_LEN	0x280

char* get_ios_info_from_tmd();

/* Variables */
static const char certs_fs[] ATTRIBUTE_ALIGN(32) = "/sys/cert.sys";

u8 shutdown = 0;

void __Sys_ResetCallback(void)
{
	/* Reboot console */
	Sys_Reboot();
}

void __Sys_PowerCallback(void)
{
	/* Poweroff console */
	//Sys_Shutdown();
	shutdown = 1;
}


void Sys_Init(void)
{
	/* Initialize video subsytem */
	VIDEO_Init();

	/* Set RESET/POWER button callback */
	SYS_SetResetCallback(__Sys_ResetCallback);
	SYS_SetPowerCallback(__Sys_PowerCallback);

	/* Prepare random seed */
	srand(time(0));
}

void Sys_Reboot(void)
{
	/* Restart console */
	STM_RebootSystem();
}

void Sys_Shutdown(void)
{
	/* Poweroff console */
	if(CONF_GetShutdownMode() == CONF_SHUTDOWN_IDLE) {
		s32 ret;

		/* Set LED mode */
		ret = CONF_GetIdleLedMode();
		if(ret >= 0 && ret <= 2)
			STM_SetLedMode(ret);

		/* Shutdown to idle */
		STM_ShutdownToIdle();
	} else {
		/* Shutdown to standby */
		STM_ShutdownToStandby();
	}
}

void Sys_LoadMenu(void)
{
	/* Return to the Wii system menu */
	SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
}


s32 Sys_GetCerts(signed_blob **certs, u32 *len)
{
	static signed_blob certificates[CERTS_LEN] ATTRIBUTE_ALIGN(32);

	s32 fd, ret;

	/* Open certificates file */
	fd = IOS_Open(certs_fs, 1);
	if (fd < 0)
		return fd;

	/* Read certificates */
	ret = IOS_Read(fd, certificates, sizeof(certificates));

	/* Close file */
	IOS_Close(fd);

	/* Set values */
	if (ret > 0) {
		*certs = certificates;
		*len   = sizeof(certificates);
	}

	return ret;
}

void prep_exit()
{
	Services_Close();
	Subsystem_Close();
	extern void Video_Close();
	Video_Close();
}

void Sys_Exit()
{
	prep_exit();
	exit(0);
}

void Sys_HBC()
{
	int ret = 0;
	prep_exit();
	WII_Initialize();
	//printf("%d\n1.07\n",ret); sleep(1);
    ret = WII_LaunchTitle(TITLE_ID(0x00010001,0xAF1BF516)); // 1.07
	//printf("%d\nJODI\n",ret); sleep(1);
    ret = WII_LaunchTitle(TITLE_ID(0x00010001,0x4A4F4449)); // JODI
	//printf("%d\nHAXX\n",ret);
    ret = WII_LaunchTitle(TITLE_ID(0x00010001,0x48415858)); // HAXX
	//printf("%d\nexit\n",ret); sleep(1);
	exit(0);
}

void Sys_Channel(u32 channel)
{
		int ret = 0;
	prep_exit();
	WII_Initialize();
	//printf("%d\nJODI\n",ret); sleep(1);
    ret = WII_LaunchTitle(TITLE_ID(0x00010001,channel));
}


// mload from uloader by Hermes


#include "mload.h"
#include "mload_modules.h"

// uLoader 2.5:
#define size_ehcmodule2 20340
#define size_dip_plugin2 3304
//extern unsigned char ehcmodule2[size_ehcmodule2];
extern unsigned char dip_plugin2[size_dip_plugin2];

// uLoader 2.8D:
#define size_ehcmodule3 22264
#define size_dip_plugin3 3352
//extern unsigned char ehcmodule3[size_ehcmodule3];
extern unsigned char dip_plugin3[size_dip_plugin3];

// uLoader 3.0B:
//#define size_ehcmodule4 32384
//#define size_dip_plugin4 3512
// uLoader 3.0C:
//#define size_ehcmodule4 32432
// uLoader 3.1:
#define size_ehcmodule4 32400
//#define size_dip_plugin4 3080 // uloader 3.1
#define size_dip_plugin4 3224 // uloader 3.2
//extern unsigned char ehcmodule4[size_ehcmodule4];
extern unsigned char dip_plugin4[size_dip_plugin4];
// EHCFAT module:
//#define size_ehcmodule_frag 70529 // cfg50-52
#define size_ehcmodule_frag 70613 // cfg53
//#include "../ehcsize.h"
extern unsigned char ehcmodule_frag[size_ehcmodule_frag];
int mload_ehc_fat = 0;
int mload_need_fat = 0;

// current
void *ehcmodule;
int size_ehcmodule;

void *dip_plugin;
int size_dip_plugin;

// external2
char ehc_path[200];
void *external_ehcmodule2 = NULL;
int size_external_ehcmodule2 = 0;

// external3
char ehc_path3[200];
void *external_ehcmodule3 = NULL;
int size_external_ehcmodule3 = 0;

// external4
char ehc_path4[200];
void *external_ehcmodule4 = NULL;
int size_external_ehcmodule4 = 0;

// external_fat
char ehc_path_fat[200];
void *external_ehcmodule_frag = NULL;
int size_external_ehcmodule_frag = 0;

static u32 ios_36[16] ATTRIBUTE_ALIGN(32)=
{
	0, // DI_EmulateCmd
	0,
	0x2022DDAC, // dvd_read_controlling_data
	0x20201010+1, // handle_di_cmd_reentry (thumb)
	0x20200b9c+1, // ios_shared_alloc_aligned (thumb)
	0x20200b70+1, // ios_shared_free (thumb)
	0x20205dc0+1, // ios_memcpy (thumb)
	0x20200048+1, // ios_fatal_di_error (thumb)
	0x20202b4c+1, // ios_doReadHashEncryptedState (thumb)
	0x20203934+1, // ios_printf (thumb)
};

static u32 ios_38[16] ATTRIBUTE_ALIGN(32)=
{
	0, // DI_EmulateCmd
	0,
	0x2022cdac, // dvd_read_controlling_data
	0x20200d38+1, // handle_di_cmd_reentry (thumb)
	0x202008c4+1, // ios_shared_alloc_aligned (thumb)
	0x20200898+1, // ios_shared_free (thumb)
	0x20205b80+1, // ios_memcpy (thumb)
	0x20200048+1, // ios_fatal_di_error (thumb)
	0x20202874+1, // ios_doReadHashEncryptedState (thumb)
	0x2020365c+1, // ios_printf (thumb)
};


u32 patch_datas[8] ATTRIBUTE_ALIGN(32);

data_elf my_data_elf;
int my_thread_id=0;

void load_ext_ehc_module(int verbose)
{
	if(!external_ehcmodule2)
	{
		snprintf(ehc_path, sizeof(ehc_path), "%s/ehcmodule.elf", USBLOADER_PATH);
		size_external_ehcmodule2 = Fat_ReadFile(ehc_path, &external_ehcmodule2);
		if (size_external_ehcmodule2 < 0) {
			size_external_ehcmodule2 = 0;
		}
	}
	if(!external_ehcmodule3)
	{
		snprintf(ehc_path3, sizeof(ehc_path3), "%s/ehcmodule3.elf", USBLOADER_PATH);
		size_external_ehcmodule3 = Fat_ReadFile(ehc_path3, &external_ehcmodule3);
		if (size_external_ehcmodule3 < 0) {
			size_external_ehcmodule3 = 0;
		}
	}
	if(!external_ehcmodule4)
	{
		snprintf(ehc_path4, sizeof(ehc_path4), "%s/ehcmodule4.elf", USBLOADER_PATH);
		size_external_ehcmodule4 = Fat_ReadFile(ehc_path4, &external_ehcmodule4);
		if (size_external_ehcmodule4 < 0) {
			size_external_ehcmodule4 = 0;
		}
	}
	if(!external_ehcmodule_frag)
	{
		snprintf(ehc_path_fat, sizeof(ehc_path_fat), "%s/ehcmodule_frag.elf", USBLOADER_PATH);
		size_external_ehcmodule_frag = Fat_ReadFile(ehc_path_fat, &external_ehcmodule_frag);
		if (size_external_ehcmodule_frag < 0) {
			size_external_ehcmodule_frag = 0;
		}
	}
}

static char mload_ver_str[40];

void mk_mload_version()
{
	mload_ver_str[0] = 0;
	if (CFG.ios_mload
			|| (is_ios_type(IOS_TYPE_WANIN) && IOS_GetRevision() >= 18) )
	{
		if (IOS_GetRevision() >= 4) {
			if (is_ios_type(IOS_TYPE_WANIN)) {
				char *info = get_ios_info_from_tmd();
				if (info) {
					sprintf(mload_ver_str, "Base: IOS%s ", info);
				} else {
					sprintf(mload_ver_str, "Base: IOS?? DI:%d ", wanin_mload_get_IOS_base());
				}
			} else {
				sprintf(mload_ver_str, "Base: IOS%d ", mload_get_IOS_base());
			}
		}
		if (IOS_GetRevision() > 4) {
			int v, s;
			v = mload_get_version();
			s = v & 0x0F;
			v = v >> 4;
			sprintf(mload_ver_str + strlen(mload_ver_str), "mload v%d.%d ", v, s);
		} else {
			sprintf(mload_ver_str + strlen(mload_ver_str), "mload v%d ", IOS_GetRevision());
		}
	}
}

void print_mload_version()
{
	int new_wanin = is_ios_type(IOS_TYPE_WANIN) && IOS_GetRevision() >= 18;
	if (CFG.ios_mload || new_wanin) {
		printf("%s", mload_ver_str);
	}
}

int load_ehc_module_ex(int verbose)
{
	int is_ios=0;

	//extern int wbfs_part_fat;
	mload_ehc_fat = 0;
	if (mload_need_fat)
	{
		if (verbose) {
			if (strncasecmp(CFG.partition, "ntfs", 4) == 0) {
				printf("[NTFS]");
			} else {
				printf("[FAT]");
			}
		}
		if (IOS_GetRevision() < 4) {
			printf("\nERROR: IOS%u rev%u\n", IOS_GetVersion(), IOS_GetRevision());
			printf(gt("FAT mode only supported with ios 222 rev4"));
			printf("\n");
			sleep(5);
			return -1;
		}
		mload_ehc_fat = 1;
		size_ehcmodule = size_ehcmodule_frag;
		ehcmodule = ehcmodule_frag;
		size_dip_plugin = size_dip_plugin4;
		dip_plugin = dip_plugin4;
		if(external_ehcmodule_frag) {
			printf("\n");
			printf_("external: %s\n", ehc_path_fat);
			printf_("");
			ehcmodule = external_ehcmodule_frag;
			size_ehcmodule = size_external_ehcmodule_frag;
		}
	}
	else if (IOS_GetRevision() <= 2)
	{
		size_dip_plugin = size_dip_plugin2;
		dip_plugin = dip_plugin2;
		//size_ehcmodule = size_ehcmodule2;
		//ehcmodule = ehcmodule2;
		if(external_ehcmodule2) {
			//if (verbose) {
				printf("\n");
				printf_("external: %s\n", ehc_path);
				printf_("");
			//}
			ehcmodule = external_ehcmodule2;
			size_ehcmodule = size_external_ehcmodule2;
		} else {
			printf("\n");
			printf(gt("ERROR: ehcmodule2 no longer included!\n"
						"external file ehcmodule.elf must be used!"));
			printf("\n");
			sleep(5);
			return -1;
		}

	} else if (IOS_GetRevision() == 3) {
			size_dip_plugin = size_dip_plugin3;
			dip_plugin = dip_plugin3;
			//size_ehcmodule = size_ehcmodule3;
			//ehcmodule = ehcmodule3;
			if(external_ehcmodule3) {
				//if (verbose) {
					printf("\n");
					printf_("external: %s\n", ehc_path3);
					printf_("");
				//}
				ehcmodule = external_ehcmodule3;
				size_ehcmodule = size_external_ehcmodule3;
			} else {
				printf("\n");
				printf(gt("ERROR: ehcmodule3 no longer included!\n"
							"external file ehcmodule3.elf must be used!"));
				printf("\n");
				sleep(5);
				return -1;
			}
	} else if (IOS_GetRevision() == 4) {
		size_dip_plugin = size_dip_plugin4;
		dip_plugin = dip_plugin4;
		//size_ehcmodule = size_ehcmodule4;
		//ehcmodule = ehcmodule4;
		// use ehcmodule_frag also for wbfs
		size_ehcmodule = size_ehcmodule_frag;
		ehcmodule = ehcmodule_frag;
		if(external_ehcmodule4) {
			//if (verbose) {
				printf("\n");
				printf_("external: %s\n", ehc_path4);
				printf_("");
			//}
			ehcmodule = external_ehcmodule4;
			size_ehcmodule = size_external_ehcmodule4;
		}
	} else if (IOS_GetRevision() >= 5) {
		size_dip_plugin = size_dip_plugin4;
		dip_plugin = dip_plugin4;
		size_ehcmodule = size_ehcmodule_frag;
		ehcmodule = ehcmodule_frag;
	/*
	} else {
		printf("\nERROR: IOS%u rev%u not supported\n",
				IOS_GetVersion(), IOS_GetRevision());
		sleep(5);
		return -1;
	*/
	}

	if (IOS_GetRevision() >= 4) {

		// NEW
		int ret = load_ehc_module();
		if (ret == 0) mk_mload_version();
		mload_close();
		return ret;
	}

	if (IOS_GetRevision() <= 2) {

		if (mload_module(ehcmodule, size_ehcmodule)<0) return -1;

	} else {

		if(mload_init()<0) return -1;
		mload_elf((void *) ehcmodule, &my_data_elf);
		my_thread_id= mload_run_thread(my_data_elf.start, my_data_elf.stack, my_data_elf.size_stack, my_data_elf.prio);
		if(my_thread_id<0) return -1;

	}

	usleep(350*1000);

	// Test for IOS
	
	mload_seek(0x20207c84, SEEK_SET);
	mload_read(patch_datas, 4);
	if(patch_datas[0]==0x6e657665) 
		{
		is_ios=38;
		}
	else
		{
		is_ios=36;
		}

	if(is_ios==36)
		{
		// IOS 36
		memcpy(ios_36, dip_plugin, 4);		// copy the entry_point
		memcpy(dip_plugin, ios_36, 4*10);	// copy the adresses from the array
		
		mload_seek(0x1377E000, SEEK_SET);	// copy dip_plugin in the starlet
		mload_write(dip_plugin,size_dip_plugin);

		// enables DIP plugin
		mload_seek(0x20209040, SEEK_SET);
		mload_write(ios_36, 4);
		 
		}
	if(is_ios==38)
		{
		// IOS 38

		memcpy(ios_38, dip_plugin, 4);	    // copy the entry_point
		memcpy(dip_plugin, ios_38, 4*10);   // copy the adresses from the array
		
		mload_seek(0x1377E000, SEEK_SET);	// copy dip_plugin in the starlet
		mload_write(dip_plugin,size_dip_plugin);

		// enables DIP plugin
		mload_seek(0x20208030, SEEK_SET);
		mload_write(ios_38, 4);

		}
	
	mk_mload_version();

	mload_close();

return 0;
}

// Reload custom ios

#include "usbstorage.h"
#include "sdhc.h"
#include "fat.h"
#include "wbfs.h"
#include "restart.h"
#include "wpad.h"
#include "wdvd.h"
extern s32 wbfsDev;


int ReloadIOS(int subsys, int verbose)
{
	int ret = -1;
	int sd_m = fat_sd_mount;
	int usb_m = fat_usb_mount;

	if (CURR_IOS_IDX == CFG.game.ios_idx
		&& is_ios_type(IOS_TYPE_WANIN)) return 0;
	
	if (verbose) {
		printf_("IOS(%d) ", CFG.ios);
		if (CFG.ios_mload) printf("mload ");
		else if (CFG.ios_yal) printf("yal ");
	}

	mload_need_fat = 0;
	if (strncasecmp(CFG.partition, "fat", 3) == 0) {
		mload_need_fat = 1;
	}
	if (strncasecmp(CFG.partition, "ntfs", 4) == 0) {
		mload_need_fat = 1;
	}
	if (wbfsDev == WBFS_DEVICE_SDHC) {
		if (CFG.ios_mload)
		{
			// wbfs on sd with 222/223 requires fat module
			mload_need_fat = 1;
		}
	}

	if (CURR_IOS_IDX == CFG.game.ios_idx
		&& mload_need_fat == mload_ehc_fat)
	{
		if (verbose) {
			if (mload_ehc_fat) {
				if (strncasecmp(CFG.partition, "ntfs", 4) == 0) {
					printf("[NTFS]");
				} else {
					printf("[FAT]");
				}
			}
			printf("\n\n");
		}
		return 0;
	}

	*mload_ver_str = 0;

	if (CFG.ios_mload) {
		load_ext_ehc_module(verbose);
	}
	
	if (verbose) printf(".");

	// Close storage
	Fat_UnmountAll();
	USBStorage_Deinit();
	SDHC_Close();

	// Close subsystems
	if (subsys) {
		Subsystem_Close();
		WDVD_Close();
	}

	/* Load Custom IOS */
	usleep(100000);
	ret = IOS_ReloadIOS(CFG.ios);
	usleep(300000);
	if (ret < 0) {
		//if (verbose) {
			printf("\n");
			printf_x(gt("ERROR:"));
			printf("\n");
			printf_(gt("Custom IOS %d could not be loaded! (ret = %d)"), CFG.ios, ret);
			printf("\n");
		//}
		goto err;
	}
	if (verbose) {
		printf(".");
	}

	// mload ehc & dip
	if (CFG.ios_mload) {
		ret = load_ehc_module_ex(verbose);
		if (ret < 0) {
			//if (verbose) {
				printf("\n");
				printf_x(gt("ERROR: Loading EHC module! (%d)"), ret);
				printf("\n");
			//}
			goto err;
		}
	}

	if (is_ios_type(IOS_TYPE_WANIN) && IOS_GetRevision() >= 18) {
		load_dip_249();
	}

	if (verbose) {
		printf(".");
		if (CFG.ios_mload) {
			printf("\n");
			printf_("");
			print_mload_version();
		}
	}

	// re-Initialize Storage
	if (sd_m) Fat_MountSDHC();
	if (usb_m) Fat_MountUSB();
	if (verbose) printf(".");

	// re-initialize subsystems
	if (subsys) {
		// wpad
		Wpad_Init();
		// storage
		if (wbfsDev == WBFS_DEVICE_USB) {
			USBStorage_Init();
		}
		if (wbfsDev == WBFS_DEVICE_SDHC) {
			SDHC_Init();
		}
		// init dip
		ret = Disc_Init();
		if (ret < 0) {
			//if (verbose) {
				printf("\n");
				printf_x(gt("ERROR:"));
				printf("\n");
				printf_(gt("Could not initialize DIP module! (ret = %d)"), ret);
				printf("\n");
			//}
			goto err;
		}
	}
	if (verbose) printf(".\n");
	
	CURR_IOS_IDX = CFG.game.ios_idx;

	return 0;

err:
	if (subsys) {
		Wpad_Init();
		Restart_Wait();
	}
	return ret;
}

void Block_IOS_Reload()
{
	if (CFG.ios_mload) {
		if (CFG.game.block_ios_reload) {
			patch_datas[0]=*((u32 *) (dip_plugin+16*4));
			mload_set_ES_ioctlv_vector((void *) patch_datas[0]);
			printf_(gt("IOS Reload: Blocked"));
			printf("\n");
			sleep(1);
		}
	}
}

u8 BCA_Data[64] ATTRIBUTE_ALIGN(32);
int have_bca_data = 0;

void load_bca_data(u8 *discid)
{
	if (CFG.disable_bca) return;
	char filename[100];
	int size = 0;
	void *buf = NULL;
	// ID6
	snprintf(D_S(filename), "%s/%.6s.bca", USBLOADER_PATH, (char*)discid);
	size = Fat_ReadFile(filename, &buf);
	if (size <= 0) {
		// ID4
		snprintf(D_S(filename), "%s/%.4s.bca", USBLOADER_PATH, (char*)discid);
		size = Fat_ReadFile(filename, &buf);
	}
	if (size < 64) {
		SAFE_FREE(buf);
		return;
	}
	printf_("BCA: %s\n", filename);
	memcpy(BCA_Data, buf, 64);
	have_bca_data = 1;
	SAFE_FREE(buf);
}

void insert_bca_data()
{
	if (!have_bca_data) return;
	if (!CFG.ios_mload || IOS_GetRevision() < 4)
	{
		printf(gt("ERROR: CIOS222/223 v4 required for BCA"));
		printf("\n");
		sleep(2);
		return;
	}
	// offset 15 (bca_data area)
	mload_seek(*((u32 *) (dip_plugin+15*4)), SEEK_SET);
	mload_write(BCA_Data, 64);
	mload_close();
}


// WANINKOKO DIP PLUGIN

#if 0
void save_dip()
{
	//int dip_buf[0x5000/sizeof(int)];
	void *dip_buf = memalign(32,0x5000);
	int dip_size = 4000;
	printf("saving dip\n");
	if(mload_init()<0) {
		printf("mload init\n");
		sleep(3);
		return;
	}
	u32 base;
	int size;
	mload_get_load_base(&base, &size);
	printf("base: %08x %x\n", base, size);
	printf("mseek\n");
	mload_seek(0x13800000, SEEK_SET);
	memset(dip_buf, 0, sizeof(dip_buf));
	printf("mread\n");
	mload_read(dip_buf, dip_size);
	printf("fopen\n");
	FILE *f = fopen("sd:/dip.bin", "wb");
	if (!f) {
		printf("fopen\n");
		sleep(3);
		return;
	}
	printf("fwrite\n");
	fwrite(dip_buf, dip_size, 1, f);
	fclose(f);
	printf("dip saved\n");
	mload_close();
	sleep(3);
	printf("unmount\n");
	Fat_UnmountSDHC();
	printf("exit\n");
	exit(0);
}

void try_hello()
{
	int ret;
	printf("mload init\n");
	if(mload_init()<0) {
		sleep(3);
		return;
	}
	u32 base;
	int size;
   	mload_get_load_base(&base, &size);
	printf("base: %08x %x\n", base, size);
	mload_close();
	printf("disc init:\n");
	ret = Disc_Init();
	printf("= %d\n", ret);
	u32 x = 6;
	s32 WDVD_hello(u32 *status);
	ret = WDVD_hello(&x);
	printf("hello: %d %x %d\n", ret, x, x);
	ret = WDVD_hello(&x);
	printf("hello: %d %x %d\n", ret, x, x);
	sleep(1);
	//printf("exit\n");
	//exit(0);
}
#endif

#ifndef size_dip249
#define size_dip249 5276
#endif

extern u8 dip_plugin_249[size_dip249];


void load_dip_249()
{
	int ret;
	if (is_ios_type(IOS_TYPE_WANIN) && IOS_GetRevision() >= 18)
	{
		printf("[FRAG]");
		if(mload_init()<0) {
			printf(" ERROR\n");
			return;
		}
		/*
		u32 base;
		int size;
		mload_get_load_base(&base, &size);
		printf("base: %08x %x\n", base, size);
		*/
		ret = mload_module(dip_plugin_249, size_dip249);
		//printf("load mod: %d\n", ret);
		mk_mload_version();
		mload_close();
		//printf("OK\n");
	}
}


int get_ios_type()
{
	switch (IOS_GetVersion()) {
		case 247:
		case 248:
		case 249:
		case 250:
			return IOS_TYPE_WANIN;
		case 222:
		case 223:
			if (IOS_GetRevision() == 1)
				return IOS_TYPE_KWIIRK;
		case 224:
			return IOS_TYPE_HERMES;
	}
	return IOS_TYPE_UNK;
}

int is_ios_type(int type)
{
	return (get_ios_type() == type);
}


s32 GetTMD(u64 TicketID, signed_blob **Output, u32 *Length)
{
    signed_blob* TMD = NULL;

    u32 TMD_Length;
    s32 ret;

    /* Retrieve TMD length */
    ret = ES_GetStoredTMDSize(TicketID, &TMD_Length);
    if (ret < 0)
        return ret;

    /* Allocate memory */
    TMD = (signed_blob*)memalign(32, (TMD_Length+31)&(~31));
    if (!TMD)
        return IPC_ENOMEM;

    /* Retrieve TMD */
    ret = ES_GetStoredTMD(TicketID, TMD, TMD_Length);
    if (ret < 0)
    {
        free(TMD);
        return ret;
    }

    /* Set values */
    *Output = TMD;
    *Length = TMD_Length;

    return 0;
}

s32 checkIOS(u32 IOS)
{
	signed_blob *TMD = NULL;
    tmd *t = NULL;
    u32 TMD_size = 0;
    u64 title_id = 0;
    s32 ret = 0;

    // Get tmd to determine the version of the IOS
    title_id = (((u64)(1) << 32) | (IOS));
    ret = GetTMD(title_id, &TMD, &TMD_size);
    
    if (ret == 0) {
        t = (tmd*)SIGNATURE_PAYLOAD(TMD);
        if (t->title_version == 65280) {
			ret = -1;
		}
    } else {
		ret = -2;
	}
    free(TMD);
    return ret;
}

// ios base detection from NeoGamma R9 (by WiiPower)


#define info_number 23

static u32 hashes[info_number][5] = {
{0x20e60607, 0x4e02c484, 0x2bbc5758, 0xee2b40fc, 0x35a68b0a},		// cIOSrev13a
{0x620c57c7, 0xd155b67f, 0xa451e2ba, 0xfb5534d7, 0xaa457878}, 		// cIOSrev13b
{0x3c968e54, 0x9e915458, 0x9ecc3bda, 0x16d0a0d4, 0x8cac7917},		// cIOS37 rev18
{0xe811bca8, 0xe1df1e93, 0x779c40e6, 0x2006e807, 0xd4403b97},		// cIOS38 rev18
{0x697676f0, 0x7a133b19, 0x881f512f, 0x2017b349, 0x6243c037},		// cIOS57 rev18
{0x34ec540b, 0xd1fb5a5e, 0x4ae7f069, 0xd0a39b9a, 0xb1a1445f},		// cIOS60 rev18
{0xd98a4dd9, 0xff426ddb, 0x1afebc55, 0x30f75489, 0x40b27ade},		// cIOS70 rev18
{0x0a49cd80, 0x6f8f87ff, 0xac9a10aa, 0xefec9c1d, 0x676965b9},		// cIOS37 rev19
{0x09179764, 0xeecf7f2e, 0x7631e504, 0x13b4b7aa, 0xca5fc1ab},		// cIOS38 rev19
{0x6010d5cf, 0x396415b7, 0x3c3915e9, 0x83ded6e3, 0x8f418d54},		// cIOS57 rev19
{0x589d6c4f, 0x6bcbd80a, 0xe768f258, 0xc53a322c, 0xd143f8cd},		// cIOS60 rev19
{0x8969e0bf, 0x7f9b2391, 0x31ecfd88, 0x1c6f76eb, 0xf9418fe6},		// cIOS70 rev19
{0x30aeadfe, 0x8b6ea668, 0x446578c7, 0x91f0832e, 0xb33c08ac},		// cIOS36 rev20
{0xba0461a2, 0xaa26eed0, 0x482c1a7a, 0x59a97d94, 0xa607773e},		// cIOS37 rev20
{0xb694a33e, 0xf5040583, 0x0d540460, 0x2a450f3c, 0x69a68148},		// cIOS38 rev20
{0xf6058710, 0xfe78a2d8, 0x44e6397f, 0x14a61501, 0x66c352cf},		// cIOS53 rev20
{0xfa07fb10, 0x52ffb607, 0xcf1fc572, 0xf94ce42e, 0xa2f5b523},		// cIOS55 rev20
{0xe30acf09, 0xbcc32544, 0x490aec18, 0xc276cee6, 0x5e5f6bab},		// cIOS56 rev20
{0x595ef1a3, 0x57d0cd99, 0x21b6bf6b, 0x432f6342, 0x605ae60d},		// cIOS57 rev20
{0x687a2698, 0x3efe5a08, 0xc01f6ae3, 0x3d8a1637, 0xadab6d48},		// cIOS60 rev20
{0xea6610e4, 0xa6beae66, 0x887be72d, 0x5da3415b, 0xa470523c},		// cIOS61 rev20
{0x64e1af0e, 0xf7167fd7, 0x0c696306, 0xa2035b2d, 0x6047c736},		// cIOS70 rev20
{0x0df93ca9, 0x833cf61f, 0xb3b79277, 0xf4c93cd2, 0xcd8eae17}		// cIOS80 rev20
};

static char infos[info_number][24] = {
{"?? rev13a"},
{"?? rev13b"},
{"37 rev18"},
{"38 rev18"},
{"57 rev18"},
{"60 rev18"},
{"70 rev18"},
{"37 rev19"},
{"38 rev19"},
{"57 rev19"},
{"60 rev19"},
{"70 rev19"},
{"36 rev20"},
{"37 rev20"},
{"38 rev20"},
{"53 rev20"},
{"55 rev20"},
{"56 rev20"},
{"57 rev20"},
{"60 rev20"},
{"61 rev20"},
{"70 rev20"},
{"80 rev20"}
};	

s32 brute_tmd(tmd *p_tmd) 
{
	u16 fill;
	for(fill=0; fill<65535; fill++) 
	{
		p_tmd->fill3 = fill;
		sha1 hash;
		SHA1((u8 *)p_tmd, TMD_SIZE(p_tmd), hash);;
		if (hash[0]==0) 
		{
			return 0;
		}
	}
	return -1;
}

char* get_ios_info_from_tmd()
{
	signed_blob *TMD = NULL;
	tmd *t = NULL;
	u32 TMD_size = 0;
	int try_ver = 0;
	u32 i;

	int ret = GetTMD((((u64)(1) << 32) | (IOS_GetVersion())), &TMD, &TMD_size);
	
	char *info = NULL;
	//static char default_info[32];
	//sprintf(default_info, "IOS%u (Rev %u)", IOS_GetVersion(), IOS_GetRevision());
	//info = (char *)default_info;

	if (ret != 0) goto out;

	t = (tmd*)SIGNATURE_PAYLOAD(TMD);

	dbg_printf("tmd id: %llx %x-%x t: %x v: %d\n",
			t->title_id, TITLE_HIGH(t->title_id), TITLE_LOW(t->title_id),
			t->title_type, t->title_version);

	// safety check
	if (t->title_id != TITLE_ID(1, IOS_GetVersion())) goto out;

	// patch title id, so hash matches
	if (t->title_id != TITLE_ID(1, 249)) {
		t->title_id = TITLE_ID(1,249);
		if (t->title_version >= 65530) { // for 250
			try_ver = t->title_version = 20;
		}
		retry_ver:
		// fake sign it
		brute_tmd(t);
	}

	sha1 hash;
	SHA1((u8 *)TMD, TMD_size, hash);

	for (i = 0; i < info_number; i++)
	{
		if (memcmp((void *)hash, (u32 *)&hashes[i], sizeof(sha1)) == 0)
		{
			info = (char *)&infos[i];
			break;
		}
	}
	if (info == NULL) {
		// not found, retry lower rev.
		if (try_ver > 13) {
			try_ver--;
			t->title_version = try_ver;
			goto retry_ver;
		}
	}

	out:
	SAFE_FREE(TMD);
    return info;
}

