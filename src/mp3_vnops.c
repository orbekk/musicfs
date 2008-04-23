/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <id3.h>
#include <sys/types.h>
#include <dirent.h>

#define MAXPATHLEN 100000

const char *musicpath = "/home/lulf/dev/mp3fs/music";

void traverse_hierarchy(char *, void (*)(char *, void *), void *);
void mp3_artist(char *, void *);


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
	char *tmp;
	char *parsepath;

	filler (buf, ".", NULL, 0);
	filler (buf, "..", NULL, 0);

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
		/* XXX: traverse full dir. */
		traverse_hierarchy(musicpath, mp3_artist, filler);
		return (0);
	}
	return (-ENOENT);
}


void
traverse_hierarchy(char *dirpath, void (*fileop)(char *, void *), void *data)
{
	DIR *dirp;
	struct dirent *dp;
	char filepath[MAXPATHLEN];
	struct stat st;

	dirp = opendir(dirpath);
	while ((dp = readdir(dirp)) != NULL) {
		if (!strcmp(dp->d_name, ".") ||
		    !strcmp(dp->d_name, ".."))
			continue;
		
		snprintf(filepath, sizeof(filepath), "%s/%s",
		    musicpath, dp->d_name);
		if (stat(filepath, &st) < 0)
			err(1, "error doing stat on %s", filepath);
		if (st.st_mode & S_IFDIR)
			traverse_hierarchy(filepath, fileop, data);
		if (!(st.st_mode & S_IFREG))
			continue;
		/* If it's a regular file, perform the operation. */
		fileop(filepath, data);
	}
	closedir(dirp);
	return (0);
}

struct filler_data {
	void *buf;
	fuse_fill_dir_t filler;
};

/* Retrieve artist name given a path. */
void
mp3_artist(char *filepath, void *data)
{
	struct filler_data *fd;
	ID3Tag *tag;
	ID3Frame *artist;
	ID3Field *field;
	fuse_fill_dir_t filler;
	void *buf;
	char name[MAXPATHLEN];

	fd = (struct filler_data *)data;
	filler = fd->filler;
	buf = fd->buf;

	tag = ID3Tag_New();
	ID3Tag_Link(tag, filepath);
	artist = ID3Tag_FindFrameWithID(tag, ID3FID_LEADARTIST);
	field = ID3Frame_GetField(artist, ID3FN_TEXT);
	ID3Field_GetASCII(field, name, ID3Field_Size(field));
	filler(buf, name, NULL, 0);
	return 0;
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
