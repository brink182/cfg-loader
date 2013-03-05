#include <gccore.h>
#include "util.h"
#include "disc.h"

char *get_name(u64 titleid);
int CHANNEL_Banner(struct discHdr *hdr, SoundInfo *snd);
u64 getChannelSize(struct discHdr *hdr);
u64 getChannelReqIos(struct discHdr *hdr);
