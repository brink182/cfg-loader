#ifndef _VERSION_H 
#define _VERSION_H 

#define STR(X) #X
#define QUOTE(X) STR(X)

#ifndef VERSION
#define VERSION 00xy
#endif

#define VERSION_STR QUOTE(VERSION)

#if defined(FAT_BUILD) && (FAT_BUILD == 1)

   #define CFG_VERSION_STR VERSION_STR"-fat" 
   #define DEFAULT_IOS_IDX CFG_IOS_222_MLOAD 
   #define CFG_DEFAULT_PARTITION "FAT1" 
   #define CFG_HIDE_HDDINFO 1 

#else 

   #define CFG_VERSION_STR VERSION_STR
   #define DEFAULT_IOS_IDX CFG_IOS_249 
   #define CFG_DEFAULT_PARTITION "WBFS1" 
   #define CFG_HIDE_HDDINFO 0 

#endif 
#endif 

