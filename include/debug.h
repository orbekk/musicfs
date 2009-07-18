/*
 * Musicfs is a FUSE module implementing a media filesystem in userland.
 * Copyright (C) 2008  Ulf Lilleengen, Kjetil Ørbekk
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * A copy of the license can typically be found in COPYING
 */

#ifndef _DEBUG_H_
#define _DEBUG_H_


#ifdef DEBUGGING
#  include <stdlib.h>
#  include <stdio.h>
#  define DEBUGPATH "debug.txt"
extern pthread_mutex_t __debug_lock__;
#  define DEBUG(...) do { \
	pthread_mutex_lock(&__debug_lock__);			       \
	fprintf (stderr, __VA_ARGS__);				       \
	pthread_mutex_unlock(&__debug_lock__);			       \
    } while (0)
#else
#  define DEBUG(...)
#endif 


#endif /* !_DEBUG_H_ */
