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

/* Scan the music initially. */
void
mp3_scan(char *filepath)
{
	TagLib_File *file;
	TagLib_Tag *tag;
	char *artist, *album, *genre, *title;
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
		ret = sqlite3_prepare_v2(handle, "SELECT * FROM artist WHERE "
		    "name=?", -1, &st, NULL);
		if (ret != SQLITE_OK) {
			warnx("Error preparing statement: %s\n",
			    sqlite3_errmsg(handle));
			break;
		}
		sqlite3_bind_text(st, 1, artist, -1, SQLITE_STATIC);
		ret = sqlite3_step(st);
		sqlite3_finalize(st);
		/* Already exists or generic error. */
		if (ret != SQLITE_DONE)
			break;
		/* Doesn't exist, so we can insert it. */
		ret = sqlite3_prepare_v2(handle, "INSERT INTO artist(name) "
		    "VALUES(?)", -1, &st, NULL);
		if (ret != SQLITE_OK) {
			warnx("Error preparing statement: %s\n",
			    sqlite3_errmsg(handle));
			break;
		}
		sqlite3_bind_text(st, 1, artist, -1, SQLITE_STATIC);
		ret = sqlite3_step(st);
		sqlite3_finalize(st);
		if (ret != SQLITE_DONE) {
			warnx("Error inserting into database: %s\n",
			    sqlite3_errmsg(handle));
			break;
		}
	} while (0);

	/* Insert genre if it doesn't exist. */
	do {
		genre = taglib_tag_genre(tag);
		if (genre == NULL)
			break;
		/* First find out if it exists. */
		ret = sqlite3_prepare_v2(handle, "SELECT * FROM genre WHERE "
		    "name=?", -1, &st, NULL);
		if (ret != SQLITE_OK) {
			warnx("Error preparing statement: %s\n",
			    sqlite3_errmsg(handle));
			break;
		}
		sqlite3_bind_text(st, 1, genre, -1, SQLITE_STATIC);
		ret = sqlite3_step(st);
		sqlite3_finalize(st);
		/* Already exists or generic error. */
		if (ret != SQLITE_DONE)
			break;
		/* Doesn't exist, so we can insert it. */
		ret = sqlite3_prepare_v2(handle, "INSERT INTO genre(name) "
		    "VALUES(?)", -1, &st, NULL);
		if (ret != SQLITE_OK) {
			warnx("Error preparing statement: %s\n",
			    sqlite3_errmsg(handle));
			break;
		}
		sqlite3_bind_text(st, 1, genre, -1, SQLITE_STATIC);
		ret = sqlite3_step(st);
		sqlite3_finalize(st);
		if (ret != SQLITE_DONE) {
			warnx("Error inserting into database: %s\n",
			    sqlite3_errmsg(handle));
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
		ret = sqlite3_prepare_v2(handle, "SELECT * FROM song, artist "
		    " WHERE artist.name=song.artistname AND title=? AND year=?",
		    -1, &st, NULL);
		if (ret != SQLITE_OK) {
			warnx("Error preparing statement: %s\n",
			    sqlite3_errmsg(handle));
			break;
		}
		sqlite3_bind_text(st, 1, title, -1, SQLITE_STATIC);
		sqlite3_bind_int(st, 2, year);
		ret = sqlite3_step(st);
		sqlite3_finalize(st);
		/* Already exists or generic error. */
		if (ret != SQLITE_DONE)
			break;
		/* Now, finally insert it. */
		ret = sqlite3_prepare_v2(handle, "INSERT INTO song(title, "
		    "artistname, album, genrename, year) VALUES(?, ?, ?, ?, ?)",
		    -1, &st, NULL);
		if (ret != SQLITE_OK) {
			warnx("Error preparing insert statement: %s\n",
			    sqlite3_errmsg(handle));
			break;
		}
		sqlite3_bind_text(st, 1, title, -1, SQLITE_STATIC);
		sqlite3_bind_text(st, 2, artist, -1, SQLITE_STATIC);
		sqlite3_bind_text(st, 3, album, -1, SQLITE_STATIC);
		sqlite3_bind_text(st, 4, genre, -1, SQLITE_STATIC);
		sqlite3_bind_int(st, 5, year);
		ret = sqlite3_step(st);
		sqlite3_finalize(st);
		if (ret != SQLITE_DONE) {
			warnx("Error inserting into database: %s\n",
			    sqlite3_errmsg(handle));
			break;
		}
	} while (0);
	taglib_tag_free_strings();
	taglib_file_free(file);
}

/*
 * Perform query and list the result from field.
 * XXX: The varargs stuff got a bit uglier than I anticipated. I think i'd like
 * to remove it perhaps...
 */
void
mp3_list(int field, struct filler_data *fd, const char *query, const char *fmt, ...)
{
	sqlite3_stmt *st;
	fuse_fill_dir_t filler;
	void *buf;
	const unsigned char *value;
	char *s;
	va_list ap;
	int d, error, ret;

	filler = fd->filler;
	buf = fd->buf;
	/* Open database. */
	error = sqlite3_open(DBNAME, &handle);
	if (error) {
		warnx("Can't open database: %s\n", sqlite3_errmsg(handle));
		sqlite3_close(handle);
		return;
	}
	ret = sqlite3_prepare_v2(handle, query, -1, &st, NULL);
	if (ret != SQLITE_OK) {
	//	warnx("Error preparing statement\n");
		return;
	}
	va_start(ap, fmt);
	int count = 1;
	while (*fmt) {
		switch (*fmt++) {
		case 's':
			s = va_arg(ap, char *);
			sqlite3_bind_text(st, count++, s, -1, SQLITE_STATIC);
			break;
		case 'd':
			d = va_arg(ap, int);
			sqlite3_bind_int(st, count++, d);
			break;
		}
	}
	va_end(ap);
	ret = sqlite3_step(st);
	while (ret == SQLITE_ROW) {
		value = sqlite3_column_text(st, field);
		filler(buf, (const char *)value, NULL, 0);
		ret = sqlite3_step(st);
	}
	// XXX: Check for errors too.
	sqlite3_close(handle);
}

/*
 * Count number of tokens in pathname.
 * XXX: should we strip the first and last?
 */
int
mp3_numtoken(const char *str)
{
	const char *ptr;
	int num;

	num = 0;
	ptr = str;
	if (*ptr == '/')
		ptr++;
	while ((*ptr != '\0')) {
		if (*ptr++ == '/')
			num++;
	}
	if (*(ptr - 1) == '/')
		num--;
	return (num + 1);
}

/*
 * Extract token number toknum and return it in escaped form.
 */
char *
mp3_gettoken(const char *str, int toknum)
{
	const char *ptr, *start, *end;
	char *ret, *dest;
	int numescape;
	int i;
	size_t size;

	start = str;
	/* Find the token start. */
	for (i = 0; i < toknum; i++) {
		start = strchr(start, '/');
		if (start == NULL)
			return (NULL);
		start++;
	}
	/* Find the end pointer. */
	end = strchr(start, '/');
	if (end == NULL)
		end = strchr(start, '\0');
	/* 
	 * Count how many bytes extra to take into account because of escaping.
	 */
	numescape = 0;
	ptr = start;
	while ((ptr = strchr(ptr, '\'')) != NULL && ptr < end) {
		ptr++;
		numescape++;
	}
	size = (end - start) + numescape;
	ret = malloc(size + 1);
	if (ret == NULL)
		return (ret);
	ptr = start;
	dest = ret;
	while (ptr < end) {
		if (*ptr == '\'')
			*dest++ = '\\';
		*dest++ = *ptr++;
	}
	ret[size] = '\0';
	return (ret);
}

void
mp3_list_artist(const char *path, struct filler_data *fd)
{
	char *name, *album;

	switch (mp3_numtoken(path)) {
	case 1:
		mp3_list(0, fd, "SELECT name FROM artist", "");
		break;
	case 2:
		/* So, now we got to find out the artist and list its albums. */
		name = mp3_gettoken(path, 2);
		if (name == NULL)
			break;
		mp3_list(0, fd, "SELECT DISTINCT album FROM song, artist "
		    "WHERE song.artistname = artist.name AND artist.name LIKE "
		    "'?'", "%s", name);
		free(name);
		break;
	case 3:
		/* List songs in an album. */
		name = mp3_gettoken(path, 2);
		if (name == NULL)
			break;
		album = mp3_gettoken(path, 3);
		if (album == NULL)
			break;
		mp3_list(0, fd, "SELECT title FROM song, artist WHERE "
		    "song.artistname = artist.name AND ARTIST.name LIKE '?' "
		    "AND song.album LIKE '?'", "%s%s", name, album);
		free(album);
		free(name);
		break;
	}
}

void
mp3_list_genre(const char *path, struct filler_data *fd)
{
	char *genre, *album;

	switch (mp3_numtoken(path)) {
	case 1:
		mp3_list(0, fd, "SELECT name FROM genre", ""); 
		break;
	case 2:
		genre = mp3_gettoken(path, 2);
		if (genre == NULL)
			break;
		mp3_list(0, fd, "SELECT DISTINCT album FROM song, genre WHERE"
		    " song.genrename = genre.name AND genre.name LIKE '?'",
		    "%s", genre);
		free(genre);
		break;
	case 3:
		genre = mp3_gettoken(path, 2);
		if (genre == NULL)
			break;
		album = mp3_gettoken(path, 3);
		if (album == NULL)
			break;
		mp3_list(0, fd, "SELECT title FROM song, genre WHERE "
		    "song.genrename = genre.name AND genre.name LIKE '?' "
		    " AND song.album LIKE '?'", "%s%s", genre, album);
		free(album);
		free(genre);
		break;
	}
}
