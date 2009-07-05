#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <asndlib.h>
#include <mp3player.h>
#include <gcmodplay.h>

#include "cfg.h"
#include "music.h"
#include "wpad.h"
#include "video.h"

#define FORMAT_MP3 1
#define FORMAT_MOD 2
static int music_format = 0;
static void *music_buf = NULL;
static FILE *music_f = NULL;
static int music_size = 0;
static MODPlay mod;
static char music_fname[200] = "";
static bool was_playing = false;

void _Music_Stop();

FILE* music_open()
{
	music_f = fopen(music_fname, "rb");
	return music_f;
}

s32 mp3_reader(void *fp, void *dat, s32 size)
{
	s32 ret, rcnt = 0;
	retry:
	if (!music_f) return -1;
	ret = fread(dat, 1, size, music_f);
	if (ret == 0 && rcnt == 0) {
		// eof - reopen
		fclose(music_f);
		music_open();
		rcnt++;
		goto retry;
	}
	return ret;
}


void _Music_Start()
{
	struct stat st;
	int ret;

	if (CFG.music == 0) {
		dbg_printf("Music: Disabled\n");
		return;
	} else {
		dbg_printf("Music: Enabled\n");
	}
	
	was_playing = false;

	if (*CFG.music_file) {
		strcpy(music_fname, CFG.music_file);
		dbg_printf("Music file: %s\n", music_fname);
		if (strlen(music_fname) < 5) return;
		if (stat(music_fname, &st)) {
			dbg_printf("File not found! %s\n", music_fname);
			return;
		}
	} else {
		// try music.mp3 or music.mod
		snprintf(music_fname, sizeof(music_fname), "%s/%s", USBLOADER_PATH, "music.mp3");
		if (stat(music_fname, &st) != 0) {
			snprintf(music_fname, sizeof(music_fname), "%s/%s", USBLOADER_PATH, "music.mod");
			if (stat(music_fname, &st) != 0) {
				dbg_printf("music.mp3 or music.mod not found!\n");
				return;
			}
		}
		dbg_printf("Music file: %s\n", music_fname);
	}

	music_size = st.st_size;
	dbg_printf("Music file size: %d\n", music_size);
	if (music_size <= 0) return;

	if (stricmp(music_fname+strlen(music_fname)-3, "mp3") == 0) {
		music_format = FORMAT_MP3;
	} else if (stricmp(music_fname+strlen(music_fname)-3, "mod") == 0) {
		music_format = FORMAT_MOD;
	} else {
		music_format = 0;
		return;
	}

	music_open();
	if (!music_f) {
		if (*CFG.music_file || CFG.debug) {
			printf("Error opening: %s\n", music_fname);
		   	sleep(2);
		}
		return;
	}

	if (music_format == FORMAT_MP3) {

		ASND_Init();
		MP3Player_Init();
		MP3Player_Volume(127); // of 256
		//ret = MP3Player_PlayBuffer(music_buf, music_size, NULL);
		ret = MP3Player_PlayFile(music_f, mp3_reader, NULL);
		dbg_printf("mp3 play: %s (%d)\n", ret?"error":"ok", ret);
		if (ret) goto err_play;
		usleep(150000); // wait 150ms and verify if playing
		if (!MP3Player_IsPlaying()) {
			err_play:
			printf("Error playing %s\n", music_fname);
			Music_Stop();
			sleep(1);
		}

	} else {

		music_buf = malloc(music_size);
		if (!music_buf) {
			printf("music file too big (%d) %s\n", music_size, music_fname);
			sleep(1);
			music_format = 0;
			return;
		}
		//printf("Loading...\n");
		ret = fread(music_buf, music_size, 1, music_f);
		//printf("size: %d %d\n", music_size, ret); sleep(2);
		fclose(music_f);
		music_f = NULL;
		if (ret != 1) {
			printf("error reading: %s (%d)\n", music_fname, music_size); sleep(2);
			free(music_buf);
			music_buf = NULL;
			music_size = 0;
			music_format = 0;
			return;
		}
		ASND_Init();
		MODPlay_Init(&mod);
		ret = MODPlay_SetMOD(&mod, music_buf);
		dbg_printf("mod play: %s (%d)\n", ret?"error":"ok", ret);
		if (ret < 0 ) {
			Music_Stop();
		} else  {
			MODPlay_SetVolume(&mod, 32,32); // fix the volume to 32 (max 64)
			MODPlay_Start(&mod); // Play the MOD
		}
	}
}

void Music_Start()
{
	_Music_Start();
	if (CFG.debug) {
		sleep(1);
		printf("\n    Press any button to continue...\n\n");
		Wpad_WaitButtons();
		Con_Clear();
	}
}

#if 0
void Music_Loop()
{
	if (music_format != FORMAT_MP3) return;
	if (!music_f || MP3Player_IsPlaying()) return;
	//dbg_printf("Looping mp3\n");
	CFG.debug = 0;
	_Music_Stop();
	_Music_Start();
}

void Music_CloseFile()
{
	if (music_format != FORMAT_MP3) return;
	if (CFG.music_cache) return;
	if (!music_f) return;
	was_playing = true;
	_Music_Stop();
}

void Music_OpenFile()
{
	if (was_playing) {
		CFG.debug = 0;
		_Music_Start();
	}
}
#endif

void _Music_Stop()
{
	if (music_format == FORMAT_MP3) {
		MP3Player_Stop();
	} else if (music_format == FORMAT_MOD) {
		if (music_buf) {
			MODPlay_Stop(&mod);
			MODPlay_Unload(&mod);   
		}
	}
	if (music_f) fclose(music_f);
	music_f = NULL;
	if (music_buf) free(music_buf);
	music_buf = NULL;
	music_size = 0;
	was_playing = false;
	music_format = 0;
}

void Music_Stop()
{
	_Music_Stop();
	ASND_End();
}

