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

#ifndef _MUSICFS_H_
#define _MUSICFS_H_

#include <fuse.h>

struct fuse_args;

int	mfs_run(int, char **);
int	mfs_init();

/*
 * The path to the sqlite database
 */
char *db_path;

/* 
 * Functions traversing the underlying filesystem and do operations on the
 * files, for instance scanning the collection.
 */
typedef void traverse_fn_t(const char *);
void traverse_hierarchy(const char *, traverse_fn_t);
traverse_fn_t mfs_scan;

/*
 * Data passed to mfs_lookup_list
 */
struct filler_data {
	void *buf;
	fuse_fill_dir_t filler;
};

enum lookup_datatype { LIST_DATATYPE_STRING = 1, LIST_DATATYPE_INT };

enum mfs_filetype { MFS_FILE, MFS_DIRECTORY };

/* A lookup functions returns 0 if it's finished. */
typedef int lookup_fn_t(void *, const char *);
/* Lookup function listing each row. */
lookup_fn_t mfs_lookup_list;
/* Lookup real pathname for a file. */
lookup_fn_t mfs_lookup_path;
/* Lookup function loading a path into DB */
lookup_fn_t mfs_lookup_load_path;

struct lookuphandle;

struct lookuphandle	*mfs_lookup_start(int, void *, lookup_fn_t *, const char *);
void			 mfs_lookup_insert(struct lookuphandle *, void *,
			     enum lookup_datatype);
void			 mfs_lookup_finish(struct lookuphandle *);

void	 mfs_lookup_artist(const char *, struct filler_data *);
void	 mfs_lookup_genre(const char *, struct filler_data *);
void	 mfs_lookup_album(const char *, struct filler_data *);
char	*mfs_gettoken(const char *, int);
int	 mfs_numtoken(const char *);
int	 mfs_realpath(const char *, char **);
int      mfs_reload_config();
char    *mfs_get_home_path(const char *);

enum mfs_filetype mfs_get_filetype(const char *);
#endif /* !_MUSICFS_H_ */
