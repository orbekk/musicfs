/* Miscellaneous subroutines for mp3fs. */
#define FUSE_USE_VERSION 26
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>

#include <sys/types.h>
#include <sys/param.h>
#include <dirent.h>
#include <fuse.h>

#include <tag_c.h>
#include <mp3fs.h>
#include <sqlite3.h>
#include <debug.h>

sqlite3 *handle;

int
mp3_initscan(char *musicpath)
{
	int error;

	/* Open database. */
	error = sqlite3_open(DBNAME, &handle);
	if (error) {
		warnx("Can't open database: %s\n", sqlite3_errmsg(handle));
		sqlite3_close(handle);
		return (-1);
	}
	traverse_hierarchy(musicpath, mp3_scan);
	sqlite3_close(handle);
	return (0);
}

/*
 * Traverse a hierarchy given a directory path. Perform fileoperations on the
 * files in the directory giving data as arguments, as well as recursing on
 * sub-directories.
 */
void
traverse_hierarchy(char *dirpath, traverse_fn_t fileop)
{
	DIR *dirp;
	struct dirent *dp;
	char filepath[MAXPATHLEN];
	struct stat st;

	dirp = opendir(dirpath);
	if (dirp == NULL)
		return;
	
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
			traverse_hierarchy(filepath, fileop);
			continue;
		}
		/* If it's a regular file, perform the operation. */
		if (st.st_mode & S_IFREG) {
			fileop(filepath);
		}
	}
	closedir(dirp);
}

static int
mp3_scan_callback(void *ignore, int argc, char **argv, char **colname)
{
	return 0;
}

/* Scan the music initially. */
void
mp3_scan(char *filepath)
{
	TagLib_File *file;
	TagLib_Tag *tag;
	char *artist, *album, *genre, *title;
	char *query, *errmsg;
	int ret;
	unsigned int year;
	sqlite3_stmt *st;

	file = taglib_file_new(filepath);
	/* XXX: errmsg. */
	if (file == NULL)
		return;
	tag = taglib_file_tag(file);
	if (tag == NULL) {
		printf("error!\n");
		return;
	}

	/* XXX: The main query code should perhaps be a bit generalized. */

	/* First insert artist if we have it. */
	do {
		artist = taglib_tag_artist(tag);
		if (artist == NULL)
			break;
		/* First find out if it exists. */
		asprintf(&query, "SELECT * FROM artist WHERE name='%s'",
		    artist);
		if (query == NULL)
			break;
		ret = sqlite3_prepare(handle, query, -1, &st, NULL);
		if (ret != SQLITE_OK) {
			warnx("Error preparing statement\n");
			free(query);
			break;
		}
		ret = sqlite3_step(st);
		sqlite3_finalize(st);
		/* Already exists or generic error. */
		if (ret != SQLITE_DONE)
			break;
		/* Doesn't exist, so we can insert it. */
		free(query);
		asprintf(&query, "INSERT INTO artist(name) VALUES('%s')",
		    artist);
		if (query == NULL)
			break;
		ret = sqlite3_exec(handle, query, mp3_scan_callback, 0,
		    &errmsg);
		free(query);
		if (ret != SQLITE_OK) {
			warnx("Error inserting into database: %s\n", errmsg);
			sqlite3_free(errmsg);
			break;
		}
	} while (0);

	/* Insert genre if it doesn't exist. */
	do {
		genre = taglib_tag_genre(tag);
		if (genre == NULL)
			break;
		/* First find out if it exists. */
		asprintf(&query, "SELECT * FROM genre WHERE name='%s'", genre);
		if (query == NULL)
			break;
		ret = sqlite3_prepare(handle, query, -1, &st, NULL);
		if (ret != SQLITE_OK) {
			warnx("Error preparing statement\n");
			free(query);
			break;
		}
		ret = sqlite3_step(st);
		sqlite3_finalize(st);
		/* Already exists or generic error. */
		if (ret != SQLITE_DONE)
			break;
		free(query);
		asprintf(&query, "INSERT INTO genre(name) VALUES('%s')", genre);
		if (query == NULL)
			break;
		ret = sqlite3_exec(handle, query, mp3_scan_callback, 0,
		    &errmsg);
		free(query);
		if (ret != SQLITE_OK) {
			warnx("Error inserting into database: %s\n", errmsg);
			sqlite3_free(errmsg);
			break;
		}
	} while (0);

	/* Finally, insert song. */
	do {
		title = taglib_tag_title(tag);
		album = taglib_tag_album(tag);
		year = taglib_tag_year(tag);
		if (title == NULL || genre == NULL || artist == NULL ||
		    album == NULL)
			break;

		/* First find out if it exists. */
		asprintf(&query, "SELECT * FROM song, artist WHERE "
		    "artist.name=song.artistname AND title='%s' AND year=%u",
		    title, year);
		if (query == NULL)
			break;
		ret = sqlite3_prepare(handle, query, -1, &st, NULL);
		if (ret != SQLITE_OK) {
			warnx("Error preparing statement\n");
			free(query);
			break;
		}
		ret = sqlite3_step(st);
		sqlite3_finalize(st);
		free(query);
		/* Already exists or generic error. */
		if (ret != SQLITE_DONE) {
			printf("Duplicate song!\n");
			break;
		}
		/* Now, finally insert it. */
		asprintf(&query, "INSERT INTO song(title, artistname, album, "
		    "genrename, year) VALUES('%s', '%s', '%s', '%s', %u)",
		    title, artist, album, genre, year);
		if (query == NULL)
			break;
		ret = sqlite3_exec(handle, query, mp3_scan_callback, 0,
		    &errmsg);
		free(query);
		if (ret != SQLITE_OK) {
			warnx("Error inserting into database: %s\n", errmsg);
			sqlite3_free(errmsg);
			break;
		}	
		
	} while (0);
	taglib_tag_free_strings();
}
