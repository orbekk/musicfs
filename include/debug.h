#ifndef _DEBUG_H_
#define _DEBUG_H_


#ifdef DEBUGGING
#  include <stdio.h>
#  define DEBUGPATH "debug.txt"
#  define DEBUG(...) FILE *__f491 = fopen(DEBUGPATH, "a"); \
                     fprintf (__f491, __VA_ARGS__);      \
                     fclose(__f491);
#else
#  define DEBUG(...)
#endif 


#endif
