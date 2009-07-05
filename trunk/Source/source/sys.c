#include <stdio.h>
#include <ogcsys.h>
#include <stdlib.h>

#include "sys.h"

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

void Subsystem_Close(void);
void Sys_Exit()
{
	Subsystem_Close();
	exit(0);
}


// mload from uloader by Hermes


#include "mload.h"

// uLoader 1.6:
//#define size_ehcmodule 18160
// uLoader 2.0:
//#define size_ehcmodule 18412
//#define size_dip_plugin 3700
// uLoader 2.1:
#define size_ehcmodule 18708
#define size_dip_plugin 3304
extern unsigned char ehcmodule[size_ehcmodule];
extern unsigned char dip_plugin[size_dip_plugin];


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

int load_ehc_module()
{
int is_ios=0;


/*
// bloquea el flag de disco dentro desde el PPC
*((volatile u32 *) 0xcd0000c4)=*((volatile u32 *) 0xcd0000c4) | (1<<7);
*((volatile u32 *) 0xcd0000c0)=(*((volatile u32 *) 0xcd0000c8))| (1<<7);

*((volatile u32 *) 0xcd0000d4)=(*((volatile u32 *) 0xcd0000d4)) & ~(1<<6);*/
/*
	if(mload_init()<0) return -1;
	mload_elf((void *) logmodule, &my_data_elf);
	my_thread_id= mload_run_thread(my_data_elf.start, my_data_elf.stack, my_data_elf.size_stack, my_data_elf.prio);
	if(my_thread_id<0) return -1;
	*/
  

	if(mload_module(ehcmodule, size_ehcmodule)<0) return -1;
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
#include "cfg.h"
#include "wdvd.h"
extern s32 wbfsDev;


int ReloadIOS(int subsys, int verbose)
{
	int ret = -1;
	extern int sd_mount_mode;
	extern int usb_mount;
	int sd_m = sd_mount_mode;
	int usb_m = usb_mount;

	if (CURR_IOS_IDX == CFG.game.ios_idx
		&& CURR_IOS_IDX == CFG_IOS_249) return 0;
	
	if (verbose) {
		printf("    IOS(%d) ", CFG.ios);
		if (CFG.ios_mload) printf("mload");
		else if (CFG.ios_yal) printf("yal");
	}

	if (CURR_IOS_IDX == CFG.game.ios_idx) {
		printf("\n\n");
		return 0;
	}
	
	if (verbose) printf(" .");

	// Close storage
	if (usb_m) {
		Fat_UnmountUSB();
		USBStorage_Deinit();
	}
	if (sd_m) {
		Fat_UnmountSDHC();
		SDHC_Close();
	}

	// Close subsystems
	if (subsys) {
		Subsystem_Close();
		WDVD_Close();
		USBStorage_Deinit();
		SDHC_Close();
	}

	/* Load Custom IOS */
	usleep(300000);
	ret = IOS_ReloadIOS(CFG.ios);
	usleep(300000);
	if (ret < 0) {
		if (verbose) {
			printf("\n[+] ERROR:\n");
			printf("    Custom IOS %d could not be loaded! (ret = %d)\n", CFG.ios, ret);
		}
		goto err;
	}
	if (verbose) printf(".");

	// mload ehc & dip
	if (CFG.ios_mload) {
		ret = load_ehc_module();
		if (ret < 0) {
			if (verbose)
				printf("\n[+] ERROR: Loading EHC module! (%d)\n", ret);
			goto err;
		}
	}
	if (verbose) printf(".");

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
			if (verbose) {
				printf("\n[+] ERROR:\n");
				printf("    Could not initialize DIP module! (ret = %d)\n", ret);
			}
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
			printf("    IOS Reload: Blocked\n");
			sleep(1);
		}
	}
}			


