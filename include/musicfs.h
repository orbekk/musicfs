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

#ifndef _MP3FS_H_
#define _MP3FS_H_
struct fuse_args;

int	mfs_run(int, char **);
int	mfs_initscan();


/* 
 * Functions traversing the underlying filesystem and do operations on the
 * files, for instance scanning the collection.
 */
typedef void traverse_fn_t(char *);
void traverse_hierarchy(const char *, traverse_fn_t);
traverse_fn_t mfs_scan;

/*
 * Data passed to mfs_lookup_list
 */
struct filler_data {
	void *buf;
	fuse_fill_dir_t filler;
};

/*
 * Data passed to mfs_lookup_open
 */
struct file_data {
	int fd;
	int found;
};

#define LIST_DATATYPE_STRING 1
#define LIST_DATATYPE_INT 2

enum mfs_filetype { MFS_NOTFOUND, MFS_FILE, MFS_DIRECTORY };

/* A lookup functions returns 0 if it's finished. */
typedef int lookup_fn_t(void *, const char *);
/* Lookup function listing each row. */
lookup_fn_t mfs_lookup_list;
/* Lookup function opening the first file match. */
lookup_fn_t mfs_lookup_open;
/* Lookup function stat'ing a file. */
lookup_fn_t mfs_lookup_stat;
/* Lookup function loading a path into DB */
lookup_fn_t mfs_lookup_load_path;

struct lookuphandle;

struct lookuphandle	*mfs_lookup_start(int, void *, lookup_fn_t *, const char *);
void			 mfs_lookup_insert(struct lookuphandle *, void *, int);
void			 mfs_lookup_finish(struct lookuphandle *);

void	 mfs_lookup_artist(const char *, struct filler_data *);
void	 mfs_lookup_genre(const char *, struct filler_data *);
void	 mfs_lookup_album(const char *, struct filler_data *);
char	*mfs_gettoken(const char *, int);
int	 mfs_numtoken(const char *);
int      mfs_file_data_for_path(const char *, void *);
int      mfs_reload_config();
char    *mfs_get_home_path(const char *);

enum mfs_filetype mfs_get_filetype(const char *);
#endif
