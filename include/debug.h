#ifndef _DEBUG_H_
#define _DEBUG_H_

#ifdef DEBUGGING
#  define DEBUG(...) fprintf (stderr, __VA_ARGS__)
#else
#  define DEBUG(...)
#endif 

#endif
