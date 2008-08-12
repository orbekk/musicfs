#ifndef _DEBUG_H_
#define _DEBUG_H_


#ifdef DEBUGGING
#  include <stdio.h>
#  define DEBUGPATH "debug.txt"
FILE *__debug_handle__;
#  define DEBUG(...) __debug_handle__ = fopen(DEBUGPATH, "a"); \
                     fprintf (__debug_handle__, __VA_ARGS__);      \
                     fclose(__debug_handle__);
#else
#  define DEBUG(...)
#endif 


#endif
