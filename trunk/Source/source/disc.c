#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <ogc/lwp_watchdog.h>

#include "apploader.h"
#include "disc.h"
#include "subsystem.h"
#include "video.h"
#include "wdvd.h"
#include "fst.h"
#include "cfg.h"
#include "music.h"

/* Constants */
#define PTABLE_OFFSET	0x40000
#define WII_MAGIC	0x5D1C9EA3

/* Disc pointers */
static u32 *buffer = (u32 *)0x93000000;
static u8  *diskid = (u8  *)0x80000000;

int yal_OpenPartition(u32 Offset);
int yal_Identify();

#define        Sys_Magic		((u32*) 0x80000020)
#define        Version			((u32*) 0x80000024)
#define        Arena_L			((u32*) 0x80000030)
#define        Bus_Speed		((u32*) 0x800000f8)
#define        CPU_Speed		((u32*) 0x800000fc)

void __Disc_SetLowMem(void)
{
	/* Setup low memory */
	*(vu32 *)0x80000030 = 0x00000000; // Arena Low
	*(vu32 *)0x80000060 = 0x38A00040;
	*(vu32 *)0x800000E4 = 0x80431A80;
	*(vu32 *)0x800000EC = 0x81800000; // Dev Debugger Monitor Address
	*(vu32 *)0x800000F4 = 0x817E5480;
	*(vu32 *)0x800000F8 = 0x0E7BE2C0; // bus speed
	*(vu32 *)0x800000FC = 0x2B73A840; // cpu speed

	/* Copy disc ID */
	// online check code, seems offline games clear it?
	memcpy((void *)0x80003180, (void *)0x80000000, 4);

	// from yal (kwiirk)
	// Patch in info missing from apploader reads
	*Sys_Magic	= 0x0d15ea5e;
	*Version	= 1;
	*Arena_L	= 0x00000000;
	*Bus_Speed	= 0x0E7BE2C0;
	*CPU_Speed	= 0x2B73A840;
	
	// From NeoGamme R4 (WiiPower)
	*(vu32*)0x800030F0 = 0x0000001C;
	*(vu32*)0x8000318C = 0x00000000;
	*(vu32*)0x80003190 = 0x00000000;
	// Fix for Sam & Max (WiiPower)
	// (only works if started from DVD)
	//*(vu32*)0x80003184	= 0x80000000;	// Game ID Address

	/* Flush cache */
	DCFlushRange((void *)0x80000000, 0x3F00);
}

void __Disc_SetVMode(void)
{
	GXRModeObj *vmode = NULL;

	u32 progressive, tvmode, vmode_reg = 0;

	__console_flush(0);

	/* Get video mode configuration */
	progressive = (CONF_GetProgressiveScan() > 0) && VIDEO_HaveComponentCable();
	tvmode      =  CONF_GetVideo();

	/* Select video mode register */
	switch (tvmode) {
	case CONF_VIDEO_PAL:
		if (CONF_GetEuRGB60() > 0) {
			vmode_reg = 5;
			vmode     = (progressive) ? &TVNtsc480Prog : &TVEurgb60Hz480IntDf;
		} else
			vmode_reg = 1;

		break;

	case CONF_VIDEO_MPAL:
		vmode_reg = 4;
		break;

	case CONF_VIDEO_NTSC:
		vmode_reg = 0;
		break;
	}

	if (CFG.game.video == CFG_VIDEO_GAME) // game default
	{
	/* Select video mode */
	switch(diskid[3]) {
	/* PAL */
	case 'D':
	case 'F':
	case 'P':
	case 'X':
	case 'Y':
		if (tvmode != CONF_VIDEO_PAL) {
			vmode_reg = 1;
			vmode     = (progressive) ? &TVNtsc480Prog : &TVNtsc480IntDf;
		}

		break;

	/* NTSC or unknown */
	case 'E':
	case 'J':
		if (tvmode != CONF_VIDEO_NTSC) {
			vmode_reg = 0;
			vmode     = (progressive) ? &TVNtsc480Prog : &TVEurgb60Hz480IntDf;
		}

		break;
	}
	}

	/* Set video mode register */
	*(vu32 *)0x800000CC = vmode_reg;

	/* Set video mode */
	if (vmode)
		Video_Configure(vmode);

	/* Clear screen */
	Video_Clear(COLOR_BLACK);
}

void __Disc_SetTime(void)
{
	/* Extern */
	extern void settime(long long);

	/* Set proper time */
	settime(secs_to_ticks(time(NULL) - 946684800));
}

s32 __Disc_FindPartition(u64 *outbuf)
{
	u64 offset = 0, table_offset = 0;

	u32 cnt, nb_partitions;
	s32 ret;

	/* Read partition info */
	ret = WDVD_UnencryptedRead(buffer, 0x20, PTABLE_OFFSET);
	if (ret < 0)
		return ret;

	/* Get data */
	nb_partitions = buffer[0];
	table_offset  = buffer[1] << 2;

	/* Read partition table */
	ret = WDVD_UnencryptedRead(buffer, 0x20, table_offset);
	if (ret < 0)
		return ret;

	/* Find game partition */
	for (cnt = 0; cnt < nb_partitions; cnt++) {
		u32 type = buffer[cnt * 2 + 1];

		/* Game partition */
		if(!type)
			offset = buffer[cnt * 2] << 2;
	}

	/* No game partition found */
	if (!offset)
		return -1;

	/* Set output buffer */
	*outbuf = offset;

	return 0;
}


s32 Disc_Init(void)
{
	/* Init DVD subsystem */
	return WDVD_Init();
}

s32 Disc_Open(void)
{
	s32 ret;

	/* Reset drive */
	ret = WDVD_Reset();
	if (ret < 0)
		return ret;

	/* Read disc ID */
	return WDVD_ReadDiskId(diskid);
}

s32 Disc_Wait(void)
{
	u32 cover = 0;
	s32 ret;

	/* Wait for disc */
	while (!(cover & 0x2)) {
		/* Get cover status */
		ret = WDVD_GetCoverStatus(&cover);
		if (ret < 0)
			return ret;
	}

	return 0;
}

s32 Disc_SetWBFS(u32 mode, u8 *id)
{
	if (CFG.ios_mload) {
		//extern int _wbfs_cur_part;
		//return MLOAD_SetWBFSMode(id, _wbfs_cur_part);
		// wbfs part has to be the index of wbfs part
		// not index in mbr 
		return MLOAD_SetWBFSMode(id, 0);
	}
	if (CFG.ios_yal) {
		return YAL_Enable_WBFS(id);
	}
	/* Set WBFS mode */
	return WDVD_SetWBFSMode(mode, id);
}

s32 Disc_ReadHeader(void *outbuf)
{
	/* Read disc header */
	return WDVD_UnencryptedRead(outbuf, sizeof(struct discHdr), 0);
}

s32 Disc_IsWii(void)
{
	struct discHdr *header = (struct discHdr *)buffer;

	s32 ret;

	/* Read disc header */
	ret = Disc_ReadHeader(header);
	if (ret < 0)
		return ret;

	/* Check magic word */
	if (header->magic != WII_MAGIC)
		return -1;

	return 0;
}

s32 Disc_BootPartition(u64 offset)
{
	entry_point p_entry;

	s32 ret;

	/* Open specified partition */

	if (CFG.ios_yal) {
		ret = yal_OpenPartition(offset);
	} else {
		ret = WDVD_OpenPartition(offset);
	}
	if (ret < 0) {
		printf("ERROR: OpenPartition(0x%llx) %d\n", offset, ret);
		return ret;
	}

	// OCARINA STUFF - FISHEARS
	if (CFG.game.ocarina) {
		char gameid[8];
		memset(gameid, 0, 8);
		//memcpy(gameid, (char*)0x80000000, 6);
		memcpy(gameid, (char*)diskid, 6);
		// if not found or not confirmed, disable hooks
		CFG.game.ocarina = do_sd_code(gameid);
		printf("\n");
	}

	/* Setup low memory */
	__Disc_SetLowMem();

	/* Run apploader */
	ret = Apploader_Run(&p_entry);
	if (ret < 0) {
		printf("ERROR: Apploader %d\n", ret);
		return ret;
	}

	/* Set an appropiate video mode */
	__Disc_SetVMode();

	/* Set time */
	__Disc_SetTime();
	
	/* Close subsystems */
	Subsystem_Close();

	if (CFG.ios_yal) yal_Identify();

	/* Shutdown IOS */
 	SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);

	/* Jump to entry point */
	p_entry();

	/* Epic failure */
	while (1);

	return 0;
}

s32 Disc_WiiBoot(void)
{
	u64 offset;
	s32 ret;

	/* Find game partition offset */
	ret = __Disc_FindPartition(&offset);
	if (ret < 0)
		return ret;

	/* Boot partition */
	return Disc_BootPartition(offset);
}



// from 'yal' by kwiirk



#define ALIGNED(x) __attribute__((aligned(x)))

#define CERTS_SIZE	0xA00
static const char certs_fs[] ALIGNED(32) = "/sys/cert.sys";

static signed_blob* Certs		= 0;
static signed_blob* Ticket		= 0;
static signed_blob* Tmd		= 0;

static unsigned int C_Length	= 0;
static unsigned int T_Length	= 0;
static unsigned int MD_Length	= 0;

static u8	Ticket_Buffer[0x800] ALIGNED(32);
static u8	Tmd_Buffer[0x49e4] ALIGNED(32);


s32 yal_GetCerts(signed_blob** Certs, u32* Length)
{
	static unsigned char		Cert[CERTS_SIZE] ALIGNED(32);
	memset(Cert, 0, CERTS_SIZE);
	s32				fd, ret;

	fd = IOS_Open(certs_fs, ISFS_OPEN_READ);
	if (fd < 0) return fd;

	ret = IOS_Read(fd, Cert, CERTS_SIZE);
	if (ret < 0)
	{
		if (fd >0) IOS_Close(fd);
		return ret;
	}

	*Certs = (signed_blob*)(Cert);
	*Length = CERTS_SIZE;

	if (fd > 0) IOS_Close(fd);

	return 0;
}

int yal_OpenPartition(u32 Offset)
{
	int ret = -1;
	// Offset = Partition_Info.Offset << 2
	u32 Partition_Info_Offset = Offset >> 2;

	printf("    Loading .");
	
    //DI_Set_OffsetBase(Offset);
    ret = YAL_Set_OffsetBase(Offset);
	if (ret < 0) {
		printf("\nERROR: Offset(0x%x) %d\n", Offset, ret);
		return ret; 
	}

	ret = yal_GetCerts(&Certs, &C_Length);
	if (ret < 0) {
		printf("\nERROR: GetCerts %d\n", ret);
		return ret; 
	}

	//DI_Read_Unencrypted(Ticket_Buffer, 0x800, Partition_Info.Offset << 2);
	WDVD_UnencryptedRead(Ticket_Buffer, 0x800, Offset);
	Ticket		= (signed_blob*)(Ticket_Buffer);
	T_Length	= SIGNED_TIK_SIZE(Ticket);

	// Open Partition and get the TMD buffer
	//if (DI_Open_Partition(Partition_Info.Offset, 0,0,0, Tmd_Buffer) < 0)
	ret = YAL_Open_Partition(Partition_Info_Offset, 0,0,0, Tmd_Buffer);
	if (ret < 0) {
		printf("\nERROR: OpenPartition %d\n", ret);
		return ret;
	}
	Tmd = (signed_blob*)(Tmd_Buffer);
	MD_Length = SIGNED_TMD_SIZE(Tmd);

	printf(".");
	return 0;
}


int yal_Identify()
{
	// Identify as the game
	if (IS_VALID_SIGNATURE(Certs) 	&& IS_VALID_SIGNATURE(Tmd) 	&& IS_VALID_SIGNATURE(Ticket) 
		&&  C_Length > 0 				&& MD_Length > 0 			&& T_Length > 0)
	{
			int ret = ES_Identify(Certs, C_Length, Tmd, MD_Length, Ticket, T_Length, NULL);
			if (ret < 0)
					return ret;
	}
	return 0;
}



