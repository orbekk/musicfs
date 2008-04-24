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

#include <id3.h>
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
	ID3Tag *tag;
	ID3Frame *artist;
	ID3Field *field;
	fuse_fill_dir_t filler;
	void *buf;
	char name[MAXPATHLEN];

	/* Retrieve the filler function and pointers. */
	fd = (struct filler_data *)data;
	filler = fd->filler;
	buf = fd->buf;

	/* Get the tag. */
	tag = ID3Tag_New();
	ID3Tag_Link(tag, filepath);
	artist = ID3Tag_FindFrameWithID(tag, ID3FID_LEADARTIST);
	if (artist == NULL)
		return;
	field = ID3Frame_GetField(artist, ID3FN_TEXT);
	if (field == NULL)
		return;
	ID3Field_GetASCII(field, name, ID3Field_Size(field));
	/* XXX: doesn't show up... why? */
	filler(buf, name, NULL, 0);
	ID3Tag_Delete(tag);
	return;
}
