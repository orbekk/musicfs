/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
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

struct lookuphandle {
	sqlite3 *handle;
	sqlite3_stmt *st;
	const char *query;
	int field;
	int count;
	void *priv;
	lookup_fn_t *lookup;
};

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
	char *artist, *album, *genre, *title, *trackno;
	const char *extension;
	int ret;
	unsigned int track, year;
	sqlite3_stmt *st;
	struct stat fstat;
	
	file = taglib_file_new(filepath);
	/* XXX: errmsg. */
	if (file == NULL)
		return;
	tag = taglib_file_tag(file);
	if (tag == NULL) {
		printf("error!\n");
		return;
	}

	if (stat(filepath, &fstat) < 0) {
		perror("stat");
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
		track = taglib_tag_track(tag);
		year = taglib_tag_year(tag);
		extension = strrchr(filepath, (int)'.');
		if (extension == NULL)
			extension = "";
		else
			/* Drop the '.' */
			extension++;
		
		if (title == NULL || genre == NULL || artist == NULL ||
		    album == NULL)
			break;

		/* First find out if it exists. */
		ret = sqlite3_prepare_v2(handle, "SELECT * FROM song, artist "
		    " WHERE artist.name=song.artistname AND title=? AND year=?"
		    " AND song.album=?",
		    -1, &st, NULL);
		if (ret != SQLITE_OK) {
			warnx("Error preparing statement: %s\n",
			    sqlite3_errmsg(handle));
			break;
		}
		sqlite3_bind_text(st, 1, title, -1, SQLITE_STATIC);
		sqlite3_bind_int(st, 2, year);
		sqlite3_bind_text(st, 3, album, -1, SQLITE_STATIC);
		ret = sqlite3_step(st);
		sqlite3_finalize(st);
		if (ret == SQLITE_ROW)
			/* Already exists. XXX: Update it if mtime mismatches */
			break;
		if (ret != SQLITE_DONE)
			/* SQL Error. */
			break;
		/* Now, finally insert it. */
		ret = sqlite3_prepare_v2(handle, "INSERT INTO song(title, "
		    "artistname, album, genrename, year, track, filepath, "
		    "mtime, extension) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)",
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

		if (track) {
			trackno = malloc(sizeof(char) * 9);
			sprintf(trackno, "%02d", track);
			sqlite3_bind_text(st, 6, trackno, -1, SQLITE_TRANSIENT);
			free(trackno);
		} else {
			sqlite3_bind_text(st, 6, "", -1, SQLITE_STATIC);
		}

		sqlite3_bind_text(st, 7, filepath, -1, SQLITE_STATIC);
		sqlite3_bind_int(st, 8, fstat.st_mtime);
		sqlite3_bind_text(st, 9, extension, -1, SQLITE_STATIC);
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
 * Create a handle for listing music with a certain query. Allocate the
 * resources and return the handle.
 */
struct lookuphandle *
mp3_lookup_start(int field, void *data, lookup_fn_t *fn, const char *query)
{
	struct lookuphandle *lh;
	int ret, error;

	lh = malloc(sizeof(*lh));
	if (lh == NULL)
		return (NULL);
	lh->field = field;
	lh->lookup = fn;
	lh->priv = data;
	/* Open database. */
	error = sqlite3_open(DBNAME, &lh->handle);
	if (error) {
		warnx("Can't open database: %s\n", sqlite3_errmsg(lh->handle));
		sqlite3_close(lh->handle);
		free(lh);
		return (NULL);
	}
	ret = sqlite3_prepare_v2(lh->handle, query, -1, &lh->st, NULL);
	if (ret != SQLITE_OK) {
		free(lh);
		return (NULL);
	}
	lh->query = query;
	lh->count = 1;
	return (lh);
}

/*
 * Insert data that should be searched for in the list. The data is assumed to
 * be dynamically allocated, and will be free'd when mp3_lookup_finish is called!
 */
void
mp3_lookup_insert(struct lookuphandle *lh, void *data, int type)
{
	char *str;
	int val;

	switch (type) {
	case LIST_DATATYPE_STRING:
		str = (char *)data;
		sqlite3_bind_text(lh->st, lh->count++, str, -1, free);
		break;
	case LIST_DATATYPE_INT:
		val = *((int *)data);
		sqlite3_bind_int(lh->st, lh->count++, val);
		break;
	}
}

/*
 * Finish a statement buildup and use the lookup function to operate on the
 * returned data. Free the handle when done.
 */
void
mp3_lookup_finish(struct lookuphandle *lh)
{
	char buf[1024];
	const unsigned char *value;
	int ret, type, val, finished;

	if (lh == NULL)
		return;
	ret = sqlite3_step(lh->st);
	while (ret == SQLITE_ROW) {
		type = sqlite3_column_type(lh->st, lh->field);
		switch (type) {
		case SQLITE_INTEGER:
			val = sqlite3_column_int(lh->st, lh->field);
			snprintf(buf, sizeof(buf), "%d", val);
			finished = lh->lookup(lh->priv, (const char *)buf);
			break;
		case SQLITE_TEXT:
			value = sqlite3_column_text(lh->st, lh->field);
			finished = lh->lookup(lh->priv, (const char *)value);
			break;
		default:
			finished = 0;
			break;
//		default:
//			lh->filler(lh->buf, "UNKNOWN TYPE", NULL, 0);
		}
		if (finished)
			break;
		ret = sqlite3_step(lh->st);
	}
	// XXX: Check for errors too.
	sqlite3_finalize(lh->st);
	sqlite3_close(lh->handle);
	free(lh);
}

/*
 * Build a file_data struct for a path.
 *
 * data should be a pointer to a file handle
 * returns 0 on success
 */
int
mp3_file_data_for_path(const char *path, void *data) {
	DEBUG("getting file data for %s\n", path);
	struct file_data *fd;
	fd = (struct file_data *)data;
	struct lookuphandle *lh;
	char *artist, *album, *title;

	lh = NULL;
	fd->fd = -1;
	fd->found = 0;

	/* Open a specific track. */
	if (strncmp(path, "/Tracks", 7) == 0) {
		switch (mp3_numtoken(path)) {
		case 2:
			title = mp3_gettoken(path, 2);
			if (title == NULL)
				break;
			lh = mp3_lookup_start(0, fd, mp3_lookup_open,
			    "SELECT filepath FROM song "
			    "WHERE (artistname||' - '||title||'.'||extension) LIKE ?");
			if (lh == NULL)
				return (-EIO);
			mp3_lookup_insert(lh, title, LIST_DATATYPE_STRING);
			break;
		default:
			return (-ENOENT);
		}
	} else if (strncmp(path, "/Albums", 7) == 0) {
		switch (mp3_numtoken(path)) {
		case 3:
			album = mp3_gettoken(path, 2);
			if (album == NULL)
				break;
			title = mp3_gettoken(path, 3);
			if (title == NULL)
				break;
			lh = mp3_lookup_start(0, fd, mp3_lookup_open,
			    "SELECT filepath FROM song "
			    "WHERE (title||'.'||extension) LIKE ? AND "
			    "album LIKE ?");
			if (lh == NULL)
				return (-EIO);
			mp3_lookup_insert(lh, title, LIST_DATATYPE_STRING);
			mp3_lookup_insert(lh, album, LIST_DATATYPE_STRING);
			break;
		default:
			return (-ENOENT);
		}
	} else if (strncmp(path, "/Artists", 8) == 0) {
		switch (mp3_numtoken(path)) {
		case 4:
			artist = mp3_gettoken(path, 2);
			album  = mp3_gettoken(path, 3);
			title  = mp3_gettoken(path, 4);
			DEBUG("artist(%s) album(%s) title(%s)\n", artist, album, title);
			if (!(artist && album && title)) {
				break;
			}
			lh = mp3_lookup_start(0, fd, mp3_lookup_open,
			    "SELECT filepath FROM song WHERE artistname LIKE ? AND "
			    "album LIKE ? AND (title||'.'||extension) LIKE ?");
			if (lh == NULL)
			    return (-EIO);
			mp3_lookup_insert(lh, artist, LIST_DATATYPE_STRING);
			mp3_lookup_insert(lh, album, LIST_DATATYPE_STRING);
			mp3_lookup_insert(lh, title, LIST_DATATYPE_STRING);
			break;
		default:
			return (-ENOENT);
		}
	}

	if (lh) {
		mp3_lookup_finish(lh);
	}

	return 0;
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

/*
 * List album given a path.
 */
void
mp3_lookup_album(const char *path, struct filler_data *fd)
{
	struct lookuphandle *lh;
	char *album;

	switch (mp3_numtoken(path)) {
	case 1:
		lh = mp3_lookup_start(0, fd, mp3_lookup_list,
		    "SELECT DISTINCT album FROM song");
		break;
	case 2:
		/* So, now we got to find out the artist and list its albums. */
		album = mp3_gettoken(path, 2);
		if (album == NULL)
			break;
		lh  = mp3_lookup_start(0, fd, mp3_lookup_list,
		    "SELECT DISTINCT title||'.'||extension FROM song "
		    "WHERE album LIKE ?");
		mp3_lookup_insert(lh, album, LIST_DATATYPE_STRING);
		break;
	}
	mp3_lookup_finish(lh);
}

/*
 * List artist given a path.
 */
void
mp3_lookup_artist(const char *path, struct filler_data *fd)
{
	struct lookuphandle *lh;
	char *name, *album;

	switch (mp3_numtoken(path)) {
	case 1:
		lh = mp3_lookup_start(0, fd, mp3_lookup_list,
		    "SELECT name FROM artist");
		break;
	case 2:
		/* So, now we got to find out the artist and list its albums. */
		name = mp3_gettoken(path, 2);
		if (name == NULL)
			break;
		lh  = mp3_lookup_start(0, fd, mp3_lookup_list,
		    "SELECT DISTINCT album FROM song, "
		    "artist WHERE song.artistname = artist.name AND artist.name"
		    " LIKE ?");
		mp3_lookup_insert(lh, name, LIST_DATATYPE_STRING);
		break;
	case 3:
		/* List songs in an album. */
		name = mp3_gettoken(path, 2);
		if (name == NULL)
			break;
		album = mp3_gettoken(path, 3);
		if (album == NULL)
			break;
		lh = mp3_lookup_start(0, fd, mp3_lookup_list,
		    "SELECT title ||'.'|| extension FROM song, artist "
		    "WHERE song.artistname = artist.name AND artist.name "
		    "LIKE ? AND song.album LIKE ?");
		mp3_lookup_insert(lh, name, LIST_DATATYPE_STRING);
		mp3_lookup_insert(lh, album, LIST_DATATYPE_STRING);
		break;
	}
	mp3_lookup_finish(lh);
}

/*
 * Looks up tracks given a genre, or all genres.
 */
void
mp3_lookup_genre(const char *path, struct filler_data *fd)
{
	struct lookuphandle *lh;
	char *genre, *album;

	switch (mp3_numtoken(path)) {
	case 1:
		lh = mp3_lookup_start(0, fd, mp3_lookup_list,
		    "SELECT name FROM genre");
		break;
	case 2:
		genre = mp3_gettoken(path, 2);
		if (genre == NULL)
			break;
		lh = mp3_lookup_start(0, fd, mp3_lookup_list,
		    "SELECT DISTINCT album FROM song, "
		    "genre WHERE song.genrename = genre.name AND genre.name "
		    "LIKE ?");
		mp3_lookup_insert(lh, genre, LIST_DATATYPE_STRING);
		break;
	case 3:
		genre = mp3_gettoken(path, 2);
		if (genre == NULL)
			break;
		album = mp3_gettoken(path, 3);
		if (album == NULL)
			break;
		lh = mp3_lookup_start(0, fd, mp3_lookup_list,
		    "SELECT title||'.'||extension FROM song, genre WHERE"
		    " song.genrename = genre.name AND genre.name LIKE ? "
		    " AND song.album LIKE ?");
		mp3_lookup_insert(lh, genre, LIST_DATATYPE_STRING);
		mp3_lookup_insert(lh, album, LIST_DATATYPE_STRING);
		break;
	}
	mp3_lookup_finish(lh);
}

/*
 * Lookup function for filling given data into a filler.
 */
int
mp3_lookup_list(void *data, const char *str)
{
	struct filler_data *fd;

	fd = (struct filler_data *)data;
	fd->filler(fd->buf, str, NULL, 0);
	return (0);
}

/*
 * Lookup a file and open it if found.
 */
int
mp3_lookup_open(void *data, const char *str)
{
	struct file_data *fd;

	fd = (struct file_data *)data;
	fd->fd = open(str, O_RDONLY);
	fd->found = 1;
	return (1);
}

/*
 * Stat a file.
 * XXX: watch out for duplicates, we might stat more than once.
 */
int
mp3_lookup_stat(void *data, const char *str)
{
	struct stat *st;

	st = (struct stat *)data;
	if (stat(str, st) < 0)
		return (0);
	return (1);
}

/*
 * Guess on a filetype for a path.
 *
 * Examples:
 *
 * - /Artists/Whoever should be a directory, since it's an
 *   Album.
 *
 * - /Tracks/Whatever should be a file, since Whatever is a song
 *
 * Note: This should only be used for paths inside the mp3fs
 * directories (Artists, Tracks, ...)
 */
enum mp3_filetype
mp3_get_filetype(const char *path) {
	int tokens = mp3_numtoken(path);

	if (strncmp(path, "/Genres", 7) == 0 ||
	    strncmp(path, "/Artists", 8) == 0) {
		switch (tokens) {
		case 1:
		case 2:
		case 3:
			return (MP3_DIRECTORY);
		case 4:
			return (MP3_FILE);
		default:
			return (MP3_NOTFOUND);
		}
	} else if (strncmp(path, "/Albums", 7) == 0) {
		switch (tokens) {
		case 1:
		case 2:
			return (MP3_DIRECTORY);
		case 3:
			return (MP3_FILE);
		default:
			return (MP3_NOTFOUND);
		}
	} else if (strncmp(path, "/Tracks", 7) == 0) {
		switch (tokens) {
		case 1:
			return (MP3_DIRECTORY);
		case 2:
			return (MP3_FILE);
		default:
			return (MP3_NOTFOUND);
		}
	} else {
		return (MP3_NOTFOUND);
	}
}
