/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

static int mp3_getattr (const char *path, struct stat *stbuf)
{
	memset (stbuf, 0, sizeof (struct stat));
	if (strcmp (path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}
	if (strcmp (path, "/MP3Z") == 0) {
		stbuf->st_mode	= S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size	= 12;
		return 0;
	}

	return -ENOENT;
}

static int mp3_readdir (const char *path, void *buf, fuse_fill_dir_t filler,
						off_t offset, struct fuse_file_info *fi)
{
	if (strcmp (path, "/") != 0)
		return -ENOENT;

	filler (buf, ".", NULL, 0);
	filler (buf, "..", NULL, 0);
	filler (buf, "MP3Z", NULL, 0);

	return 0;
}

static int mp3_open (const char *path, struct fuse_file_info *fi)
{
	if (strcmp (path, "/MP3Z") == 0)
		return 0;

	return -ENOENT;
}

static int mp3_read (const char *path, char *buf, size_t size, off_t offset,
					 struct fuse_file_info *fi)
{
	if (strcmp (path, "/MP3Z") == 0) {
		memcpy (buf, "Oh you wish\n", 12);
		return 12;
	}

	return -ENOENT;
}

static struct fuse_operations mp3_oper = {
	.getattr	= mp3_getattr,
	.readdir	= mp3_readdir,
	.open		= mp3_open,
	.read		= mp3_read,
};
