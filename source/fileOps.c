/*////////////////////////////////////////////////////////////////////////////////////////

fsop contains coomprensive set of function for file and folder handling

en exposed s_fsop fsop structure can be used by callback to update operation status

////////////////////////////////////////////////////////////////////////////////////////*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <ogcsys.h>
#include <ogc/lwp_watchdog.h>

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h> //for mkdir 
#include <sys/statvfs.h>

#define GB_SIZE         1073741824.0

#include "fileOps.h"
#include "video.h"
#include "gettext.h"
#include "util.h"

s_fsop fsop;

void __Copy_Spinner(s32 x, s32 max)
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
	size = (123456565 / GB_SIZE);

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

// read a file from disk
u8 *fsop_ReadFile (char *path, size_t bytes2read, size_t *bytesReaded)
{
	FILE *f;
	size_t size = 0;
	
	f = fopen(path, "rb");
	if (!f)
	{
		if (bytesReaded) *bytesReaded = size;
		return NULL;
	}

	//Get file size
	fseek( f, 0, SEEK_END);
	size = ftell(f);
	
	if (size == 0) 
	{
		fclose (f);
		return NULL;
	}
	
	// goto to start
	fseek( f, 0, SEEK_SET);
	
	if (bytes2read > 0 && bytes2read < size)
		size = bytes2read;
	
	u8 *buff = malloc (size);
	size = fread (buff, 1, size, f);
	fclose (f);
	
	if (bytesReaded) *bytesReaded = size;

	return buff;
}

// write a buffer to disk
bool fsop_WriteFile (char *path, u8 *buff, size_t len)
{
	FILE *f;
	size_t size = 0;
	
	f = fopen(path, "wb");
	if (!f)
	{
		return false;
	}

	size = fwrite (buff, 1, len, f);
	fclose (f);

	if (size == len) return true;
	return false;
}

// return false if the file doesn't exist
bool fsop_GetFileSizeBytes (char *path, size_t *filesize)	// for me stats st_size report always 0 :(
{
	FILE *f;
	size_t size = 0;
	
	f = fopen(path, "rb");
	if (!f)
	{
		if (filesize) *filesize = size;
		return false;
	}

	//Get file size
	fseek( f, 0, SEEK_END);
	size = ftell(f);
	if (filesize) *filesize = size;
	fclose (f);
	
	
	return true;
}

/*

*/
u32 fsop_CountDirItems (char *source)
{
	DIR *pdir;
	struct dirent *pent;
	u32 count = 0;
	
	pdir=opendir(source);
	
	while ((pent=readdir(pdir)) != NULL) 
	{
		// Skip it
		if (strcmp (pent->d_name, ".") == 0 || strcmp (pent->d_name, "..") == 0)
			continue;

		count++;
	}
	
	closedir(pdir);
	
	return count;
}

/*
Recursive fsop_GetFolderBytes
*/
u64 fsop_GetFolderBytes (char *source, fsopCallback vc)
{
	DIR *pdir;
	struct dirent *pent;
	char newSource[300];
	u64 bytes = 0;
	
	pdir=opendir(source);
	
	while ((pent=readdir(pdir)) != NULL) 
	{
		// Skip it
		if (strcmp (pent->d_name, ".") == 0 || strcmp (pent->d_name, "..") == 0)
			continue;
			
		sprintf (newSource, "%s/%s", source, pent->d_name);
		
		// If it is a folder... recurse...
		if (fsop_DirExist (newSource))
		{
			bytes += fsop_GetFolderBytes (newSource, vc);
		}
		else	// It is a file !
		{
			size_t s;
			fsop_GetFileSizeBytes (newSource, &s);
			bytes += s;
		}
	}
	
	closedir(pdir);
	
	//Debug ("fsop_GetFolderBytes (%s) = %llu", source, bytes);
	
	return bytes;
	}

u32 fsop_GetFolderKb (char *source, fsopCallback vc)
{
	u32 ret = (u32) round ((double)fsop_GetFolderBytes (source, vc) / 1000.0);

	return ret;
}

u32 fsop_GetFreeSpaceKb (char *path) // Return free kb on the device passed
{
	struct statvfs s;
	
	statvfs (path, &s);
	
	u32 ret = (u32)round( ((double)s.f_bfree / 1000.0) * s.f_bsize);
	
	return ret ;
}


bool fsop_StoreBuffer (char *fn, u8 *buff, int size, fsopCallback vc)
{
	FILE * f;
	int bytes;
	
	f = fopen(fn, "wb");
	if (!f) return false;
	
	bytes = fwrite (buff, 1, size, f);
	fclose(f);
	
	if (bytes == size) return true;
	
	return false;
}

bool fsop_FileExist (char *fn)
{
	FILE * f;
	f = fopen(fn, "rb");
	if (!f) return false;
	fclose(f);
	return true;
}
	
bool fsop_DirExist (char *path)
{
	DIR *dir;
	
	dir=opendir(path);
	if (dir)
	{
		closedir(dir);
		return true;
	}
	
	return false;
}

bool fsop_CopyFile (char *source, char *target, fsopCallback vc)
{
	int err = 0;
	fsop.breakop = 0;
	
	u8 *buff = NULL;
	u32 size;
	u32 bytes, rb,wb;
	u32 block = 65536;
	FILE *fs = NULL, *ft = NULL;
	
	if (strstr (source, "usb:") && strstr (target, "usb:"))
	{
		block = 1024*1048;
	}
	
	fs = fopen(source, "rb");
	if (!fs)
	{
		return false;
	}

	ft = fopen(target, "wt");
	if (!ft)
	{
		fclose (fs);
		return false;
	}
	
	//Get file size
	fseek ( fs, 0, SEEK_END);
	size = ftell(fs);

	fsop.size = size;
	
	if (size == 0)
	{
		fclose (fs);
		fclose (ft);
		return true;
	}
		
	// Return to beginning....
	fseek( fs, 0, SEEK_SET);
	
	buff = memalign (32, block);
	if (buff == NULL) 
	{
		fclose (fs);
		return false;
	}
	
	bytes = 0;
	bool spinnerFlag = false;
	if (strstr (source, "game.iso")) {
		__Copy_Spinner(bytes, size);
		spinnerFlag = true;
	}
	do
	{
		rb = fread(buff, 1, block, fs );
		wb = fwrite(buff, 1, rb, ft );
		
		if (wb != wb) err = 1;
		if (rb == 0) err = 1;
		bytes += rb;
		
		if (spinnerFlag) __Copy_Spinner(bytes, size);
		
		fsop.multy.bytes += rb;
		fsop.bytes = bytes;
		
		if (vc) vc();
		if (fsop.breakop) break;
	}
	while (bytes < size && err == 0);

	fclose (fs);
	fclose (ft);
	
	free(buff);
	
	if (err) unlink (target);

	if (fsop.breakop || err) return false;
	
	return true;
}

/*
Semplified folder make
*/
int fsop_MakeFolder (char *path)
{
	if (mkdir(path, S_IREAD | S_IWRITE) == 0) return true;
	
	return false;
}

/*
Recursive copyfolder
*/
static bool doCopyFolder (char *source, char *target, fsopCallback vc)
{
	DIR *pdir;
	struct dirent *pent;
	char newSource[300], newTarget[300];
	bool ret = true;
	
	// If target folder doesn't exist, create it !
	if (!fsop_DirExist (target))
	{
		fsop_MakeFolder (target);
	}

	pdir=opendir(source);
	
	while ((pent=readdir(pdir)) != NULL && ret == true) 
	{
		// Skip it
		if (strcmp (pent->d_name, ".") == 0 || strcmp (pent->d_name, "..") == 0)
			continue;
			
		sprintf (newSource, "%s/%s", source, pent->d_name);
		sprintf (newTarget, "%s/%s", target, pent->d_name);
		
		// If it is a folder... recurse...
		if (fsop_DirExist (newSource))
		{
			ret = doCopyFolder (newSource, newTarget, vc);
		}
		else	// It is a file !
		{
			strcpy (fsop.op, pent->d_name);
			ret = fsop_CopyFile (newSource, newTarget, vc);
		}
	}
	
	closedir(pdir);

	return ret;
}
	
bool fsop_CopyFolder (char *source, char *target, fsopCallback vc)
{
	fsop.breakop = 0;
	fsop.multy.startms = ticks_to_millisecs(gettime());
	fsop.multy.bytes = 0;
	fsop.multy.size = fsop_GetFolderBytes (source, vc);
	
	//return false;
	
	//fsop_KillFolderTree (target, vc);
	
	return doCopyFolder (source, target, vc);
}
	
/*
Recursive copyfolder
*/
bool fsop_KillFolderTree (char *source, fsopCallback vc)
{
	DIR *pdir;
	struct dirent *pent;
	char newSource[300];
	
	pdir=opendir(source);
	
	while ((pent=readdir(pdir)) != NULL) 
	{
		// Skip it
		if (strcmp (pent->d_name, ".") == 0 || strcmp (pent->d_name, "..") == 0)
			continue;
			
		sprintf (newSource, "%s/%s", source, pent->d_name);
		
		// If it is a folder... recurse...
		if (fsop_DirExist (newSource))
		{
			fsop_KillFolderTree (newSource, vc);
		}
		else	// It is a file !
		{
			sprintf (fsop.op, "Removing %s", pent->d_name);
			unlink (newSource);
			
			if (vc) vc();
		}
	}
	
	closedir(pdir);
	
	unlink (source);
	
	return true;
}
	

// Pass  <mount>://<folder1>/<folder2>.../<folderN> or <mount>:/<folder1>/<folder2>.../<folderN>
bool fsop_CreateFolderTree (char *path)
{
	int i;
	int start, len;
	char buff[300];
	char b[8];
	char *p;
	
	start = 0;
	
	strcpy (b, "://");
	p = strstr (path, b);
	if (p == NULL)
	{
		strcpy (b, ":/");
		p = strstr (path, b);
		if (p == NULL)
			return false; // path must contain
	}
	
	start = (p - path) + strlen(b);
	
	len = strlen(path);

	for (i = start; i <= len; i++)
	{
		if (path[i] == '/' || i == len)
		{
			strcpy (buff, path);
			buff[i] = 0;

			fsop_MakeFolder(buff);
		}
	}
	
	// Check if the tree has been created
	return fsop_DirExist (path);
}
	
	
// Count the number of folder in a full path. It can be path1/path2/path3 or <mount>://path1/path2/path3
int fsop_CountFolderTree (char *path)
{
	int i;
	int start, len;
	char b[8];
	char *p;
	int count = 0;
	
	start = 0;
	
	strcpy (b, "://");
	p = strstr (path, b);
	if (p == NULL)
	{
		strcpy (b, ":/");
		p = strstr (path, b);
	}
	
	if (p == NULL)
		start = 0;
	else
		start = (p - path) + strlen(b);
	
	len = strlen(path);
	if (path[len-1] == '/') len--;
	if (len <= 0) return 0;

	for (i = start; i <= len; i++)
	{
		if (path[i] == '/' || i == len)
		{
			count ++;
		}
	}
	
	// Check if the tree has been created
	return count;
}

