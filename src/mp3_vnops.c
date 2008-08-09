/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
#define FUSE_USE_VERSION 26
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <err.h>

#include <sys/types.h>
#include <dirent.h>
#include <fuse.h>
#include <sys/param.h>

#include <tag_c.h>
#include <mp3fs.h>
#include <debug.h>

char musicpath[MAXPATHLEN]; // = "/home/lulf/dev/mp3fs/music";
char *logpath = "/home/lulf/dev/mp3fs/mp3fs.log";

static int mp3_getattr (const char *path, struct stat *stbuf)
{
	int tokens;
	memset (stbuf, 0, sizeof (struct stat));
	if (strcmp (path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}
	if (strcmp(path, "/Artists") == 0 ||
	    strcmp(path, "/Genres") == 0 ||
	    strcmp(path, "/Tracks") == 0 ||
	    strcmp(path, "/Albums") == 0) {
		stbuf->st_mode	= S_IFDIR | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size	= 12;
		return 0;
	}
	tokens = mp3_numtoken(path);
	if (tokens == 2) {
		stbuf->st_mode = S_IFDIR | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = 12;
		return 0;
	} else if (tokens == 3) {
		stbuf->st_mode = S_IFDIR | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = 12;
		return 0;
	} else if (tokens == 4) {
		stbuf->st_mode = S_IFREG | 0644;
		stbuf->st_nlink = 1;
		stbuf->st_size = 512;
		return 0;
	}
	return -ENOENT;
}


static int mp3_readdir (const char *path, void *buf, fuse_fill_dir_t filler,
						off_t offset, struct fuse_file_info *fi)
{
	struct filler_data fd;
	struct listhandle *lh;

	filler (buf, ".", NULL, 0);
	filler (buf, "..", NULL, 0);
	fd.buf = buf;
	fd.filler = filler;

	if (!strcmp(path, "/")) {
		filler(buf, "Artists", NULL, 0);
		filler(buf, "Genres", NULL, 0);
		filler(buf, "Tracks", NULL, 0);
		filler(buf, "Albums", NULL, 0);
		return (0);
	}

	/*
	 * 1. Parse the path.
	 * 2. Find the mp3s that matches the tags given from the path.
	 * 3. Return the list of those mp3s.
	 */
	if (strncmp(path, "/Artists", 8) == 0) {
		mp3_list_artist(path, &fd);
		return (0);
	} else if (strncmp(path, "/Genres", 7) == 0) {
		mp3_list_genre(path, &fd);
		return (0);
	} else if (strcmp(path, "/Tracks") == 0) {
		lh = mp3_list_start(0, &fd, "SELECT title FROM song");
		mp3_list_finish(lh);
		return (0);
	} else if (strcmp(path, "/Albums") == 0) {
		lh = mp3_list_start(0, &fd, "SELECT DISTINCT album FROM song");
		mp3_list_finish(lh);
		return (0);
	}

	return (-ENOENT);
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

static int mp3fs_opt_proc (void *data, const char *arg, int key,
						   struct fuse_args *outargs)
{
	static int musicpath_set = 0;

	if (key == FUSE_OPT_KEY_NONOPT && !musicpath_set) {
		/* The source directory isn't already set, let's do it */
		strcpy(musicpath, arg);
		musicpath_set = 1;
		return (0);
	}
	return (1);
}

int
mp3_run(int argc, char **argv)
{
	int ret;
	/*
	 * XXX: Build index of mp3's.
	 */

	/* Update tables. */
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <musicfolder> <mountpoint>\n", argv[0]); 
		return (-1);
	}

	struct fuse_args args = FUSE_ARGS_INIT (argc, argv);

	if (fuse_opt_parse(&args, NULL, NULL, mp3fs_opt_proc) != 0)
		exit (1);
		
	DEBUG("musicpath: %s\n", musicpath);
	mp3_initscan(musicpath);

	ret = 0;
	ret = fuse_main(args.argc, args.argv, &mp3_ops, NULL);
	return (ret);
}
