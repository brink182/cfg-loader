#ifndef _SORT_H_
#define _SORT_H_


#ifdef __cplusplus
extern "C"
{
#endif

#include "disc.h"

struct Sorts 
{
	char cfg_val[20];
	char name[100];
	int (*sortAsc) (const void *, const void *);
	int (*sortDsc) (const void *, const void *);
};

int (*default_sort_function) (const void *a, const void *b);

int filter_features(struct discHdr *list, int cnt, char *feature, bool requiredOnly);
int filter_online(struct discHdr *list, int cnt, char *ignore, bool notused);
int filter_play_count(struct discHdr *list, int cnt, char *ignore, bool notused);
int filter_controller(struct discHdr *list, int cnt, char *controller, bool requiredOnly);
int filter_genre(struct discHdr *list, int cnt, char *genre, bool notused);
int filter_games(int (*filter) (struct discHdr *, int, char *, bool), char * name, bool num);
void showAllGames(void);
void sortList(int (*sortFunc) (const void *, const void *));
void __set_default_sort(void);
int Menu_Filter(void);
int Menu_Filter2(void);
int Menu_Filter3(void);
int Menu_Sort(void);

int get_accesory_id(char *accessory);
int get_feature_id(char *feature);

#define featureCnt 4
#define accessoryCnt 17
#define genreCnt 14
#define sortCnt 9

extern s32 sort_index;
extern bool sort_desc;
extern struct Sorts sortTypes[sortCnt];


#ifdef __cplusplus
}
#endif

#endif
