#ifndef _DISC_H_
#define _DISC_H_

#define GC_GAME_SIZE 0x57058000

/* Disc header structure */
struct discHdr
{
	/* Game ID */
	u8 id[6];

	/* Game version */
	u16 version;

	/* Audio streaming */
	u8 streaming;
	u8 bufsize;

	/* Padding */
	u8 unused1[14];

	/* Magic word */
	u32 magic;

	/* Padding */
	u8 unused2[4];

	/* Game title */
	char title[64];

	/* Encryption/Hashing */
	u8 encryption;
	u8 h3_verify;

	/* Padding */
	u8 unused3[30];
	
	char folder[0xFF];
} ATTRIBUTE_PACKED;

struct gc_discHdr
{
	/* Game ID */
	char id[6];

	/* Game version */
	u16 version;

	/* Audio streaming */
	u8 streaming;
	u8 bufsize;

	/* Padding */
	u8 unused1[18];

	/* Magic word */
	u32 magic;

	/* Game title */
	char title[64];
		
	/* Padding */
	u8 unused2[64];
};

struct dml_Game
{
	/* Game ID */
	u8 id[6];

	/* Game version */
	u8 disc;

	/* Game title */
	char title[64];

	char folder[0xFF];
};

/* Prototypes */
s32  Disc_Init(void);
s32  Disc_Open(void);
s32  Disc_Wait(void);
s32  Disc_SetWBFS(u32, u8 *);
s32  Disc_ReadHeader(void *);
s32  Disc_IsWii(void);
s32  Disc_IsGC(void);
s32  Disc_BootPartition(u64, bool dvd);
s32  Disc_WiiBoot(bool dvd);
s32  Disc_DumpGCGame();

u32 appentrypoint;
#endif

