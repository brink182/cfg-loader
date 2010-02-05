#ifndef _NET_H
#define _NET_H

bool Init_Net();
void Net_Close(int close_wc24);
void Download_Cover(char *id, bool missing_only, bool verbose);
void Download_All_Covers(bool missing_only);

#endif

