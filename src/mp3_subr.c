/* Miscellaneous subroutines for mp3fs. */
#define FUSE_USE_VERSION 26
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <err.h>

#include <sys/types.h>
#include <dirent.h>
#include <fuse.h>
#include <sys/param.h>

#include <tag_c.h>
#include <mp3fs.h>

/*
 * Traverse a hierarchy given a directory path. Perform fileoperations on the
 * files in the directory giving data as arguments, as well as recursing on
 * sub-directories.
 */
void
traverse_hierarchy(char *dirpath, traverse_fn_t fileop, void *data)
{
	DIR *dirp;
	struct dirent *dp;
	char filepath[MAXPATHLEN];
	struct stat st;
	struct filler_data *fd;

	fd = data;
	dirp = opendir(dirpath);
	if (dirp == NULL) {
		fd->filler(fd->buf, "lolerr", NULL, 0);
		return;
	}
	
	/* Loop through all files. */
	while ((dp = readdir(dirp)) != NULL) {
		if (!strcmp(dp->d_name, ".") ||
		    !strcmp(dp->d_name, ".."))
			continue;
		
		snprintf(filepath, sizeof(filepath), "%s/%s",
		    dirpath, dp->d_name);
		if (stat(filepath, &st) < 0)
			err(1, "error doing stat on %s", filepath);
		/* Recurse if it's a directory. */
		if (st.st_mode & S_IFDIR)  {
			traverse_hierarchy(filepath, fileop, data);
			continue;
		}
		/* If it's a regular file, perform the operation. */
		if (st.st_mode & S_IFREG) {
			fileop(filepath, data);
		}
	}
	closedir(dirp);
}

/* 
 * Retrieve artist name given a file, and put in a buffer with a passed filler
 * function
 */
void
mp3_artist(char *filepath, void *data)
{
	struct filler_data *fd;
	fuse_fill_dir_t filler;
	void *buf;
	char name[MAXPATHLEN];
	TagLib_File *tagfile;
	TagLib_Tag  *tag;

	/* Retrieve the filler function and pointers. */
	fd = (struct filler_data *)data;
	filler = fd->filler;
	buf = fd->buf;

	/* Get the tag. */
	tagfile = taglib_file_new(filepath);
	tag     = taglib_file_tag(tagfile);

	char *artist = taglib_tag_artist(tag);
	if (artist == NULL)
		return;

	strcpy(name, artist);

	filler(buf, name, NULL, 0);

	taglib_tag_free_strings();
	taglib_file_free(tagfile);
	return;
}

