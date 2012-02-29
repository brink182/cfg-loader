#ifdef __cplusplus
extern "C"
{
#endif

#ifndef GC_H_
#define GC_H_
#include "disc.h"
void set_video_mode(int i);
void set_language(u8 lang);
s32 DML_RemoveGame(struct discHdr header);
bool DML_GameIsInstalled(u8 *discid);
#endif //GC_H_

#ifdef __cplusplus
}
#endif