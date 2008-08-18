/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

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

#define FUSE_USE_VERSION 26
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <err.h>

#include <sys/types.h>
#include <sys/time.h>
#include <dirent.h>
#include <fuse.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <unistd.h>

#include <tag_c.h>
#include <musicfs.h>
#include <debug.h>

static int mfs_getattr (const char *path, struct stat *stbuf)
{
	char *realpath;

	int status = 0;
	memset (stbuf, 0, sizeof (struct stat));

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}

	if (strcmp(path, "/.config") == 0) {
		char *mfsrc = mfs_get_home_path(".mfsrc");
		if (mfsrc == NULL)
			return (-ENOMEM);
		int res = stat(mfsrc, stbuf);
		DEBUG("stat result for %s: %d\n", mfsrc, res);
		free(mfsrc);
		return (res);
	}

	enum mfs_filetype type = mfs_get_filetype(path);
	switch (type) {
	case MFS_DIRECTORY:
		stbuf->st_mode = S_IFDIR | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = 12;
		return 0;

	case MFS_FILE:
		realpath = NULL;
		status = mfs_realpath(path, &realpath);
		if (status != 0)
			return status;
		if (stat(realpath, stbuf) != 0) {
			free(realpath);
			return -ENOENT;
		}
		free(realpath);
		return 0;

	case MFS_NOTFOUND:
	default:
		return -ENOENT;
	}
}


static int mfs_readdir (const char *path, void *buf, fuse_fill_dir_t filler,
						off_t offset, struct fuse_file_info *fi)
{
	struct filler_data fd;
	struct lookuphandle *lh;

	filler (buf, ".", NULL, 0);
	filler (buf, "..", NULL, 0);
	fd.buf = buf;
	fd.filler = filler;

	if (!strcmp(path, "/")) {
		filler(buf, "Artists", NULL, 0);
		filler(buf, "Genres", NULL, 0);
		filler(buf, "Tracks", NULL, 0);
		filler(buf, "Albums", NULL, 0);
		filler(buf, ".config", NULL, 0);
		return (0);
	}

	/*
	 * 1. Parse the path.
	 * 2. Find the mp3s that matches the tags given from the path.
	 * 3. Return the list of those mp3s.
	 */
	if (strncmp(path, "/Artists", 8) == 0) {
		mfs_lookup_artist(path, &fd);
		return (0);
	} else if (strncmp(path, "/Genres", 7) == 0) {
		mfs_lookup_genre(path, &fd);
		return (0);
	} else if (strcmp(path, "/Tracks") == 0) {
		lh = mfs_lookup_start(0, &fd, mfs_lookup_list,
		    "SELECT DISTINCT artistname||' - '||title||'.'||extension "
		    "FROM song");
		mfs_lookup_finish(lh);
		return (0);
	} else if (strncmp(path, "/Albums", 7) == 0) {
		mfs_lookup_album(path, &fd);
		return (0);
	}

	return (-ENOENT);
}

static int mfs_open (const char *path, struct fuse_file_info *fi)
{
	char *realpath;
	int fd, status;

	if (strcmp(path, "/.config") == 0)
		return (0);

	status = mfs_realpath(path, &realpath);
	if (status != 0)
		return status;
	fd = open(realpath, O_RDONLY);
	free(realpath);
	if (fd < 0)
		return (-errno);
	fi->fh = (uint64_t)fd;
	return (0);
}

static int mfs_read (const char *path, char *buf, size_t size, off_t offset,
					 struct fuse_file_info *fi)
{
	int fd;
	size_t bytes;

	DEBUG("read: path(%s) offset(%d) size(%d)\n", path, (int)offset,
		  (int)size);
	if (strcmp(path, "/.config") == 0) {
		char *mfsrc = mfs_get_home_path(".mfsrc");
		if (mfsrc == NULL)
			return (-ENOMEM);
		int fd = open(mfsrc, O_RDONLY);
		free(mfsrc);
		lseek(fd, offset, SEEK_SET);
		bytes = read(fd, buf, size);
		close(fd);
		return (bytes);
	}

	fd = (int)fi->fh;
	if (fd < 0)
		return (-EIO);
	lseek(fd, offset, SEEK_SET);
	bytes = read(fd, buf, size);
	return (bytes);

	/*
	 * 1. Find the mnode given the path. If not in cache, read through mp3
	 *    list to find it. 
	 * 2. Read the offset of the mp3 and return the data.
	 */
}

static int mfs_write(const char *path, const char *buf, size_t size,
					 off_t offset, struct fuse_file_info *fi)
{
	size_t bytes;

	DEBUG("write: path(%s) offset(%d) size(%d)\n", path, (int)offset,
		  (int)size);

	if (strcmp(path, "/.config") == 0) {
		char *mfsrc = mfs_get_home_path(".mfsrc");
		if (mfsrc == NULL)
			return (-ENOMEM);
		int fd = open(mfsrc, O_WRONLY);
		free(mfsrc);
		lseek(fd, offset, SEEK_SET);
		bytes = write(fd, buf, size);
		close(fd);
		return (bytes);
	}

	return (-1);	
}

static int mfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	DEBUG("create %s\n", path);
	return (-1);
}

static int mfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	DEBUG("mknod %s\n", path);
	return (-1);
}

static int mfs_truncate(const char *path, off_t size)
{
	char *mfsrc;
	int res;
	DEBUG("truncating %s to size %d\n", path, (int)size);

	if (strcmp(path, "/.config") == 0) {
		mfsrc = mfs_get_home_path(".mfsrc");
		if (mfsrc == NULL)
			return (-ENOMEM);
		res = truncate(mfsrc, size);
		DEBUG("truncated %s with result: %d\n", mfsrc, res)
		free(mfsrc);
		return (res);
	}

	return (-1);
}

static int mfs_flush(const char *path, struct fuse_file_info *fi)
{
	DEBUG("flushing path %s\n", path);
	if (strcmp(path, "/.config") == 0) {
		return (0);
	}

	return (-1);
}

static int mfs_fsync(const char *path, int datasync,
					 struct fuse_file_info *fi)
{
	DEBUG("fsync path %s\n", path);
	return (0);
}

static int mfs_release(const char *path, struct fuse_file_info *fi)
{
	int fd;

	DEBUG("release %s\n", path);

	if (strcmp(path, "/.config") == 0) {
		/* Reload configuration file */
		mfs_reload_config();
	}

	fd = (int)fi->fh;
	if (fd > 0)
		close(fd);

	return (0);
}

static int mfs_chmod(const char *path, mode_t mode)
{
	char *mfsrc;
	int ret;

	DEBUG("chmod %s, %d\n", path, (int)mode);
	if (strcmp(path, "/.config") == 0) {
		mfsrc = mfs_get_home_path(".mfsrc");
		if (mfsrc == NULL)
			return (-ENOMEM);
		ret = chmod(mfsrc, mode);
		free(mfsrc);
		return (ret);
	}
	
	return (-1);
}

static int mfs_utimens(const char *path, const struct timespec tv[2])
{
	char *mfsrc;
	int ret;

	DEBUG("utime %s\n", path);
	
	if (strcmp(path, "/.config") == 0) {
		mfsrc = mfs_get_home_path(".mfsrc");
		if (mfsrc == NULL)
			return (-ENOMEM);
		ret = 0; /* utimes(mfsrc, tval); */
		free(mfsrc);
		return (ret);
	}

	return (-1);
}

static int mfs_access(const char *path, int mask)
{
	DEBUG("access called for path %s\n", path);
	return (0);
}

static int mfs_setxattr(const char *path, const char *name,
						const char *val, size_t size, int flags)
{
	DEBUG("setxattr on path %s: %s=%s\n", path, name, val);
	return (0);
}

static struct fuse_operations mfs_ops = {
	.getattr    = mfs_getattr,
	.readdir    = mfs_readdir,
	.open       = mfs_open,
	.read       = mfs_read,
	.write      = mfs_write,
	.mknod      = mfs_mknod,
	.create     = mfs_create,
	.truncate   = mfs_truncate,
	.fsync      = mfs_fsync,
	.chmod      = mfs_chmod,
	.release    = mfs_release,
	.utimens    = mfs_utimens,
	.setxattr   = mfs_setxattr,
};

static int musicfs_opt_proc (void *data, const char *arg, int key,
						   struct fuse_args *outargs)
{
	/* Using config now. This is how we added the additional parameter: */
	/* 	static int musicpath_set = 0; */
	/* 	if (key == FUSE_OPT_KEY_NONOPT && !musicpath_set) { */
	/* 		/\* The source directory isn't already set, let's do it *\/ */
	/* 		strcpy(musicpath, arg); */
	/* 		musicpath_set = 1; */
	/* 		return (0); */
	/* 	} */

	return (1);
}

int
mfs_run(int argc, char **argv)
{
	int ret;

	if (argc < 1) {
		fprintf(stderr, "Usage: %s <mountpoint>\n", argv[0]); 
		return (-1);
	}

	struct fuse_args args = FUSE_ARGS_INIT (argc, argv);

	if (fuse_opt_parse(&args, NULL, NULL, musicfs_opt_proc) != 0)
		exit (1);
		
	mfs_initscan();

	ret = 0;
	ret = fuse_main(args.argc, args.argv, &mfs_ops, NULL);
	return (ret);
}
