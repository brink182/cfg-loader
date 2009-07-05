#ifndef _UTIL_H
#define _UTIL_H

#define STRCOPY(DEST,SRC) strcopy(DEST,SRC,sizeof(DEST)) 
char* strcopy(char *dest, char *src, int size);
bool str_replace(char *str, char *olds, char *news, int size);

#define dbg_printf if (CFG.debug) printf
void dbg_time1();
unsigned dbg_time2(char *msg);

#define D_S(A) A, sizeof(A)

void* LARGE_memalign(size_t align, size_t size);
void LARGE_free(void *ptr);

#define SAFE_FREE(P) if(P){free(P);P=NULL;}

#endif

