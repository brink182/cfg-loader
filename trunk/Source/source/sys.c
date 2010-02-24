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

/* Constants */
#define CERTS_LEN	0x280

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

#define TITLE_ID(x,y)        (((u64)(x) << 32) | (y))

void Sys_HBC()
{
	int ret = 0;
	prep_exit();
	WII_Initialize();
	//printf("%d\nJODI\n",ret); sleep(1);
    ret = WII_LaunchTitle(TITLE_ID(0x00010001,0x4A4F4449)); // JODI
	//printf("%d\nHAXX\n",ret);
    ret = WII_LaunchTitle(TITLE_ID(0x00010001,0x48415858)); // HAXX
	//printf("%d\nexit\n",ret); sleep(1);
	exit(0);
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
	if (CFG.ios_mload) {
		if (IOS_GetRevision() >= 4) {
			sprintf(mload_ver_str, "Base: IOS%d ", mload_get_IOS_base());
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
	if (CFG.ios_mload) {
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
		&& CURR_IOS_IDX == CFG_IOS_249) return 0;
	
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

	mload_ver_str[0] = 0;

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



