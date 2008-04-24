/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
#define FUSE_USE_VERSION 26
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <err.h>

#include <sys/types.h>
#include <dirent.h>
#include <fuse.h>
#include <sys/param.h>

#include <id3.h>
#include <mp3fs.h>

char *musicpath = "/home/lulf/dev/mp3fs/music";

static int mp3_getattr (const char *path, struct stat *stbuf)
{
	memset (stbuf, 0, sizeof (struct stat));
	if (strcmp (path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}
	if (strcmp (path, "/Artists") == 0) {
		stbuf->st_mode	= S_IFDIR | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size	= 12;
		return 0;
	}

	return -ENOENT;
}


static int mp3_readdir (const char *path, void *buf, fuse_fill_dir_t filler,
						off_t offset, struct fuse_file_info *fi)
{
	struct filler_data fd;

	filler (buf, ".", NULL, 0);
	filler (buf, "..", NULL, 0);
	fd.buf = buf;
	fd.filler = filler;

	if (!strcmp(path, "/")) {
		filler (buf, "Artists", NULL, 0);
		filler (buf, "Genres", NULL, 0);
		return (0);
	}

	/*
	 * 1. Parse the path.
	 * 2. Find the mp3s that matches the tags given from the path.
	 * 3. Return the list of those mp3s.
	 */
	if (!strcmp(path, "/Artists")) {
		/* List artists. */
		traverse_hierarchy(musicpath, mp3_artist, &fd);
		return (0);
	}
	return (-ENOENT);
}


void
call_lol(char *path)
{
	printf("Path %s\n", path);
}

static int mp3_open (const char *path, struct fuse_file_info *fi)
{
	if (strcmp (path, "/Artists") == 0)
		return 0;
	/*
	 * 1. Have a lookup cache for names?.
	 *    Parse Genre, Album, Artist etc.
	 * 2. Find MP3s that match. XXX what do we do with duplicates? just
	 *    return the first match?
	 * 3. Put the mnode of the mp3 in our cache.
	 * 4. Signal in mnode that the mp3 is being read?
	 */
	return -ENOENT;
}

static int mp3_read (const char *path, char *buf, size_t size, off_t offset,
					 struct fuse_file_info *fi)
{
	if (strcmp (path, "/Artists") == 0) {
		memcpy (buf, "Oh you wish\n", 12);
		return 12;
	}

	/*
	 * 1. Find the mnode given the path. If not in cache, read through mp3
	 *    list to find it. 
	 * 2. Read the offset of the mp3 and return the data.
	 */
	return -ENOENT;
}

static struct fuse_operations mp3_ops = {
	.getattr	= mp3_getattr,
	.readdir	= mp3_readdir,
	.open		= mp3_open,
	.read		= mp3_read,
};

int
mp3_run(int argc, char **argv)
{
	/*
	 * XXX: Build index of mp3's.
	 */

	/* Update tables. */
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <mountpoint> <musicfolder>\n", argv[0]); 
		return (-1);
	}
		
/*	musicpath = argv[2];*/
	
	return (fuse_main(argc, argv, &mp3_ops, NULL));
}
