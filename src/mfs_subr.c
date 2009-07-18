/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

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

#include <sys/types.h>
#include <sys/param.h>
#include <dirent.h>
#include <pthread.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>

#include <fusever.h>
#include <fuse.h>
#include <tag_c.h>
#include <debug.h>
#include <musicfs.h>
#include <sqlite3.h>
#include <mfs_cleanup_db.h>

#define MFS_HANDLE ((void*)-1)

#ifdef SQLITE_THREADED
#define MFS_DB_LOCK()
#define MFS_DB_UNLOCK()
#else
#define MFS_DB_LOCK() pthread_mutex_lock(&dblock)
#define MFS_DB_UNLOCK() pthread_mutex_unlock(&dblock)
#endif

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
pthread_mutex_t dblock;
pthread_mutex_t __debug_lock__;

/*
 * Returns the path to $HOME[/extra]
 */
char *mfs_get_home_path(const char *extra)
{
	int hlen, exlen;
	char *res;
	const char *home = getenv("HOME");

	if (home == NULL)
		return (NULL);
	hlen = strlen(home);
	exlen = (extra != NULL) ? strlen(extra) : 0;

	if (exlen > 0)
		asprintf(&res, "%s/%s", home, extra);
	else
		asprintf(&res, "%s", home);

	return (res);
}

/*
 * Insert a musicpath into the database.
 */
int
mfs_insert_path(const char *path, sqlite3 *handle)
{
	int res;
	sqlite3_stmt *st;

	/* Add path to registered paths in DB */
	res = sqlite3_prepare_v2(handle,
	    "SELECT path FROM path WHERE path LIKE ?",
	    -1, &st, NULL);
	if (res != SQLITE_OK) {
		DEBUG("Error preparing statement: %s\n",
		    sqlite3_errmsg(handle));
		return (-1);
	}
	sqlite3_bind_text(st, 1, path, -1, SQLITE_TRANSIENT);
	res = sqlite3_step(st);
	sqlite3_finalize(st);

	if (res == SQLITE_DONE) {
		DEBUG("Inserting path '%s' to paths\n", path);
		/* Doesn't exist. Insert it */
		res = sqlite3_prepare_v2(handle,
		    "INSERT INTO path(path, active) VALUES(?,1)",
		    -1, &st, NULL);
		if (res != SQLITE_OK) {
			DEBUG("Error preparing stamtement: %s\n",
				  sqlite3_errmsg(handle));
			return (-1);
		}
		sqlite3_bind_text(st, 1, path, -1, SQLITE_TRANSIENT);
		res = sqlite3_step(st);
		sqlite3_finalize(st);
		if (res != SQLITE_DONE) {
			DEBUG("Error inserting into database: %s\n",
			    sqlite3_errmsg(handle));
			return (-1);
		}
	}
	else {
		/* Path already in database, just activate it */
		res = sqlite3_prepare_v2(handle, "UPDATE path SET active = 1 "
		    "WHERE path LIKE ?", -1, &st, NULL);
		if (res != SQLITE_OK) {
			DEBUG("Error preparing statement: %s\n",
			    sqlite3_errmsg(handle));
			return (-1);
		}
		sqlite3_bind_text(st, 1, path, -1, SQLITE_TRANSIENT);
		res = sqlite3_step(st);
		sqlite3_finalize(st);
		if (res != SQLITE_DONE) {
			DEBUG("Error activating path '%s'\n", path);
		}
	}
	return (0);
}

/*
 * Reload the configuration from $HOME/.mfsrc
 *
 */
int
mfs_reload_config()
{
	int res, len;
	char *mfsrc = mfs_get_home_path(".mfsrc");
	char line[4096];
	struct lookuphandle *lh;
	sqlite3 *handle;
	sqlite3_stmt *st;
	FILE *f = fopen(mfsrc, "r");

	if (f == NULL) {
		DEBUG("Couldn't open configuration file %s\n",
		    mfsrc);
		return (-1);
	}

	MFS_DB_LOCK();
	res = sqlite3_open(db_path, &handle);
	if (res) {
		DEBUG("Can't open database: %s\n", sqlite3_errmsg(handle));
		sqlite3_close(handle);
		MFS_DB_UNLOCK();
		return (-1);
	}

	/* Deactivate all paths */
	res = sqlite3_prepare_v2(handle, "UPDATE path SET active = 0",
	    -1, &st, NULL);
	if (res != SQLITE_OK) {
		DEBUG("Error preparing statement: %s\n",
			  sqlite3_errmsg(handle));
		MFS_DB_UNLOCK();
		return (-1);
	}
	res = sqlite3_step(st);
	sqlite3_finalize(st);
	if (res != SQLITE_DONE) {
		DEBUG("Error deactivating paths.\n");
	}

	while (fgets(line, 4096, f) != NULL) {
		len = strlen(line);
		if (len > 0 && line[0] != '\n' && line[0] != '#') {
			if (line[len-1] == '\n')
				line[len-1] = '\0';

			res = mfs_insert_path(line, handle);
			DEBUG("inserted path %s, returned(%d)\n", line, res);
		}
	}

	mfs_cleanup_db(handle);

	free (mfsrc);
	sqlite3_close(handle);

	/* Do the actual loading */
	lh = mfs_lookup_start(0, MFS_HANDLE, mfs_lookup_load_path,
	    "SELECT path FROM path");
	mfs_lookup_finish(lh);

	MFS_DB_UNLOCK();
	return (0);
}


int
mfs_init()
{
/*	int error;*/
	db_path = mfs_get_home_path(".mfs.db");

	/* Init locks. */
	pthread_mutex_init(&dblock, NULL);
	pthread_mutex_init(&__debug_lock__, NULL);

/* 	error = mfs_insert_path(musicpath, handle); */
/* 	if (error != 0) */
/* 		return (error); */
	
	/* Scan the music initially. (Disabled, since it's really
	   annoying with huge collections) */
/* 	error = mfs_reload_config(); */
/* 	if (error != 0) { */
/* 		return (error); */
/* 	} */
	
	return (0);
}

/*
 * Traverse a hierarchy given a directory path. Perform fileoperations on the
 * files in the directory giving data as arguments, as well as recursing on
 * sub-directories.
 */
void
traverse_hierarchy(const char *dirpath, traverse_fn_t fileop)
{
	DEBUG("traversing %s\n", dirpath);
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

/* Check if a string is empty (either NULL or "") */
int
mfs_empty(const char *str)
{
	return (str == NULL || strlen(str) == 0);
}

/* Scan the music initially. */
void
mfs_scan(const char *filepath)
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
	if (file == NULL) {
		DEBUG("Unable to open file %s\n", filepath);
		return;
	}
	tag = taglib_file_tag(file);
	if (tag == NULL) {
		DEBUG("Error getting tag from %s\n", filepath);
		return;
	}

	if (stat(filepath, &fstat) < 0) {
		DEBUG("Error getting file info: %s\n", strerror(errno));
		return;
	}

	/* XXX: The main query code should perhaps be a bit generalized. */

	/* First insert artist if we have it. */
	do {
		artist = taglib_tag_artist(tag);
		if (mfs_empty(artist))
			break;
		/* First find out if it exists. */
		ret = sqlite3_prepare_v2(handle, "SELECT * FROM artist WHERE "
		    "name=?", -1, &st, NULL);
		if (ret != SQLITE_OK) {
			DEBUG("Error preparing statement: %s\n",
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
			DEBUG("Error preparing statement: %s\n",
			    sqlite3_errmsg(handle));
			break;
		}
		sqlite3_bind_text(st, 1, artist, -1, SQLITE_STATIC);
		ret = sqlite3_step(st);
		sqlite3_finalize(st);
		if (ret != SQLITE_DONE) {
			DEBUG("Error inserting into database: %s\n",
			    sqlite3_errmsg(handle));
			break;
		}
	} while (0);

	/* Insert genre if it doesn't exist. */
	do {
		genre = taglib_tag_genre(tag);
		if (mfs_empty(genre))
			break;
		/* First find out if it exists. */
		ret = sqlite3_prepare_v2(handle, "SELECT * FROM genre WHERE "
		    "name=?", -1, &st, NULL);
		if (ret != SQLITE_OK) {
			DEBUG("Error preparing statement: %s\n",
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
			DEBUG("Error preparing statement: %s\n",
			    sqlite3_errmsg(handle));
			break;
		}
		sqlite3_bind_text(st, 1, genre, -1, SQLITE_STATIC);
		ret = sqlite3_step(st);
		sqlite3_finalize(st);
		if (ret != SQLITE_DONE) {
			DEBUG("Error inserting into database: %s\n",
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
		
		if (mfs_empty(title) || mfs_empty(artist) || mfs_empty(album))
			break;

		/* First find out if it exists. */
		ret = sqlite3_prepare_v2(handle, "SELECT * FROM song, artist "
		    " WHERE artist.name=song.artistname AND title=? AND year=?"
		    " AND song.album=?",
		    -1, &st, NULL);
		if (ret != SQLITE_OK) {
			DEBUG("Error preparing statement: %s\n",
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
			DEBUG("Error preparing insert statement: %s\n",
			    sqlite3_errmsg(handle));
			break;
		}
		sqlite3_bind_text(st, 1, title, -1, SQLITE_STATIC);
		sqlite3_bind_text(st, 2, artist, -1, SQLITE_STATIC);
		sqlite3_bind_text(st, 3, album, -1, SQLITE_STATIC);
		sqlite3_bind_text(st, 4, genre, -1, SQLITE_STATIC);
		sqlite3_bind_int(st, 5, year);

		if (track) {
			asprintf(&trackno, "%02d", track);
			if (mfs_empty(trackno))
				break;
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
			DEBUG("Error inserting into database: %s\n",
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
mfs_lookup_start(int field, void *data, lookup_fn_t *fn, const char *query)
{
	struct lookuphandle *lh;
	int ret, error;

	lh = malloc(sizeof(*lh));
	if (lh == NULL)
		return (NULL);
	lh->field = field;
	lh->lookup = fn;

	/* Open database. */
	error = sqlite3_open(db_path, &lh->handle);
	if (error) {
		DEBUG("Can't open database: %s\n", sqlite3_errmsg(lh->handle));
		sqlite3_close(lh->handle);
		free(lh);
		return (NULL);
	}

	if (data == MFS_HANDLE)
		/* Workaround to give access to the handle */
		lh->priv = lh->handle;
	else
		lh->priv = data;

	ret = sqlite3_prepare_v2(lh->handle, query, -1, &lh->st, NULL);
	if (ret != SQLITE_OK) {
		free(lh);
		DEBUG("Error preparing statement: %s\n",
		    sqlite3_errmsg(handle));
		return (NULL);
	}
	lh->query = query;
	lh->count = 1;
	return (lh);
}

/*
 * Returns a new string, which is a copy of str, except that all "\'"s
 * are replaced with "'". (Needed for fetching rows containing ' in
 * the sqlite, since we are passed "\'" from FUSE)
 */
char *
mfs_escape_sqlstring(const char *str)
{
	char *p, *escaped;
	const char *q;

	int len = strlen(str) + 1;
	escaped = malloc(sizeof(char) * len);

	if (escaped == NULL)
		return (NULL);

	p = escaped;
	q = str;

	while (*q != '\0') {
		if (*q == '\\' && q[1] == '\'') {
			q++;
		}
		*p = *q;
		p++; q++;
	}
	*p = '\0';

	return escaped;
}

/*
 * Insert data that should be searched for in the list. The data is assumed to
 * be dynamically allocated, and will be free'd when mfs_lookup_finish is called!
 */
void
mfs_lookup_insert(struct lookuphandle *lh, void *data,
    enum lookup_datatype type)
{
	char *str, *escaped;
	int val;

	switch (type) {
	case LIST_DATATYPE_STRING:
		str = (char *)data;
		escaped = mfs_escape_sqlstring(str);
		free(str);
		sqlite3_bind_text(lh->st, lh->count++, escaped, -1, free);
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
mfs_lookup_finish(struct lookuphandle *lh)
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
 * Return the realpath given a path in the musicfs tree.
 *
 * Returns pointer to real path or NULL if not found.
 */
int 
mfs_realpath(const char *path, char **realpath) {
	DEBUG("getting real path for %s\n", path);
	struct lookuphandle *lh;
	char *genre, *artist, *album, *title;
	int error;

	lh = NULL;
	error = 0;

	/* Open a specific track. */
	MFS_DB_LOCK();
	if (strncmp(path, "/Tracks", 7) == 0) {
		switch (mfs_numtoken(path)) {
		case 2:
			title = mfs_gettoken(path, 2);
			if (title == NULL) {
				error = -ENOENT;
				break;
			}
			lh = mfs_lookup_start(0, realpath, mfs_lookup_path,
			    "SELECT filepath FROM song "
			    "WHERE (artistname||' - '||title||'.'||extension) LIKE ?");
			if (lh == NULL) {
				error = -EIO;
				break;
			}
			mfs_lookup_insert(lh, title, LIST_DATATYPE_STRING);
			break;
		default:
			error = -ENOENT;
		}
	} else if (strncmp(path, "/Albums", 7) == 0) {
		switch (mfs_numtoken(path)) {
		case 3:
			album = mfs_gettoken(path, 2);
			error = -ENOENT;
			if (album == NULL)
				break;
			title = mfs_gettoken(path, 3);
			if (title == NULL)
				break;
			error = 0;
			lh = mfs_lookup_start(0, realpath, mfs_lookup_path,
			    "SELECT filepath FROM song WHERE "
			    "LTRIM(track||' ')||title||'.'||extension LIKE ? "
			    "AND album LIKE ?");
			if (lh == NULL) {
				error = -EIO;
				break;
			}
			mfs_lookup_insert(lh, title, LIST_DATATYPE_STRING);
			mfs_lookup_insert(lh, album, LIST_DATATYPE_STRING);
			break;
		default:
			error = -ENOENT;
		}
	} else if (strncmp(path, "/Artists", 8) == 0) {
		switch (mfs_numtoken(path)) {
		case 4:
			artist = mfs_gettoken(path, 2);
			album  = mfs_gettoken(path, 3);
			title  = mfs_gettoken(path, 4);
			DEBUG("artist(%s) album(%s) title(%s)\n", artist, album, title);
			if (!(artist && album && title)) {
				error = -ENOENT;
				break;
			}
			lh = mfs_lookup_start(0, realpath, mfs_lookup_path,
			    "SELECT filepath FROM song WHERE artistname LIKE ? AND "
			    "album LIKE ? AND "
			    "(LTRIM(track||' ')||title||'.'||extension) LIKE ?");
			if (lh == NULL) {
				error = -EIO;
				break;
			}
			mfs_lookup_insert(lh, artist, LIST_DATATYPE_STRING);
			mfs_lookup_insert(lh, album, LIST_DATATYPE_STRING);
			mfs_lookup_insert(lh, title, LIST_DATATYPE_STRING);
			break;
		default:
			error = -ENOENT;
		}
	} else if (strncmp(path, "/Genres", 7) == 0) {
		switch (mfs_numtoken(path)) {
		case 4:
			genre = mfs_gettoken(path, 2);
			album = mfs_gettoken(path, 3);
			title = mfs_gettoken(path, 4);
			DEBUG("genre(%s) album(%s) title(%s)\n", genre, album,
			    title);
			if (!(genre && album && title)) {
				error = -ENOENT;
				break;
			}
			lh = mfs_lookup_start(0, realpath, mfs_lookup_path,
			    "SELECT filepath FROM song WHERE genrename LIKE ? "
			    "AND album LIKE ? AND "
			    "(LTRIM(track||' ')||title||'.'||extension) "
			    "LIKE ?");
			if (lh == NULL) {
				error = -EIO;
				break;
			}
			mfs_lookup_insert(lh, genre, LIST_DATATYPE_STRING);
			mfs_lookup_insert(lh, album, LIST_DATATYPE_STRING);
			mfs_lookup_insert(lh, title, LIST_DATATYPE_STRING);
			break;
		default:
			error = -ENOENT;
		}
	}

	if (lh) {
		mfs_lookup_finish(lh);
	}

	MFS_DB_UNLOCK();
	if (error != 0)
		return (error);
	if (*realpath == NULL)
		return (-ENOMEM);
	return 0;
}

/*
 * Count number of tokens in pathname.
 * XXX: should we strip the first and last?
 */
int
mfs_numtoken(const char *str)
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
mfs_gettoken(const char *str, int toknum)
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
mfs_lookup_album(const char *path, struct filler_data *fd)
{
	struct lookuphandle *lh;
	char *album;

	switch (mfs_numtoken(path)) {
	case 1:
		lh = mfs_lookup_start(0, fd, mfs_lookup_list,
		    "SELECT DISTINCT album FROM song");
		break;
	case 2:
		/* So, now we got to find out the artist and list its albums. */
		album = mfs_gettoken(path, 2);
		if (album == NULL)
			break;
		lh  = mfs_lookup_start(0, fd, mfs_lookup_list,
		    "SELECT DISTINCT LTRIM(track||' ')||title||'.'||extension "
		    "FROM song WHERE album LIKE ?");
		mfs_lookup_insert(lh, album, LIST_DATATYPE_STRING);
		break;
	}
	mfs_lookup_finish(lh);
}

/*
 * List artist given a path.
 */
void
mfs_lookup_artist(const char *path, struct filler_data *fd)
{
	struct lookuphandle *lh;
	char *name, *album;

	switch (mfs_numtoken(path)) {
	case 1:
		lh = mfs_lookup_start(0, fd, mfs_lookup_list,
		    "SELECT name FROM artist");
		break;
	case 2:
		/* So, now we got to find out the artist and list its albums. */
		name = mfs_gettoken(path, 2);
		if (name == NULL)
			break;
		lh  = mfs_lookup_start(0, fd, mfs_lookup_list,
		    "SELECT DISTINCT album FROM song, "
		    "artist WHERE song.artistname = artist.name AND artist.name"
		    " LIKE ?");
		mfs_lookup_insert(lh, name, LIST_DATATYPE_STRING);
		break;
	case 3:
		/* List songs in an album. */
		name = mfs_gettoken(path, 2);
		if (name == NULL)
			break;
		album = mfs_gettoken(path, 3);
		if (album == NULL)
			break;
		lh = mfs_lookup_start(0, fd, mfs_lookup_list,
		    "SELECT LTRIM(track||' ')||title||'.'||extension "
		    "FROM song, artist "
		    "WHERE song.artistname = artist.name AND artist.name "
		    "LIKE ? AND song.album LIKE ?");
		mfs_lookup_insert(lh, name, LIST_DATATYPE_STRING);
		mfs_lookup_insert(lh, album, LIST_DATATYPE_STRING);
		break;
	}
	mfs_lookup_finish(lh);
}

/*
 * Looks up tracks given a genre, or all genres.
 */
void
mfs_lookup_genre(const char *path, struct filler_data *fd)
{
	struct lookuphandle *lh;
	char *genre, *album;

	switch (mfs_numtoken(path)) {
	case 1:
		lh = mfs_lookup_start(0, fd, mfs_lookup_list,
		    "SELECT name FROM genre");
		break;
	case 2:
		genre = mfs_gettoken(path, 2);
		if (genre == NULL)
			break;
		lh = mfs_lookup_start(0, fd, mfs_lookup_list,
		    "SELECT DISTINCT album FROM song, "
		    "genre WHERE song.genrename = genre.name AND genre.name "
		    "LIKE ?");
		mfs_lookup_insert(lh, genre, LIST_DATATYPE_STRING);
		break;
	case 3:
		genre = mfs_gettoken(path, 2);
		if (genre == NULL)
			break;
		album = mfs_gettoken(path, 3);
		if (album == NULL)
			break;
		lh = mfs_lookup_start(0, fd, mfs_lookup_list,
		    "SELECT LTRIM(track||' ')||title||'.'||extension FROM "
		    "song, genre WHERE "
		    "song.genrename = genre.name AND genre.name LIKE ? "
		    "AND song.album LIKE ?");
		mfs_lookup_insert(lh, genre, LIST_DATATYPE_STRING);
		mfs_lookup_insert(lh, album, LIST_DATATYPE_STRING);
		break;
	}
	mfs_lookup_finish(lh);
}

/*
 * Lookup function for filling given data into a filler.
 */
int
mfs_lookup_list(void *data, const char *str)
{
	struct filler_data *fd;

	fd = (struct filler_data *)data;
	fd->filler(fd->buf, str, NULL, 0);
	return (0);
}

/*
 * Lookup the real path of a file.
 */
int
mfs_lookup_path(void *data, const char *str)
{

	char **path;
	path = data;
	*path = strdup(str);
	return (1);
}

/*
 * Load a path into database
 */
int
mfs_lookup_load_path(void *data, const char *str)
{
	handle = (sqlite3 *)data;
	traverse_hierarchy(str, mfs_scan);

	return (0);
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
 * Note: This should only be used for paths inside the musicfs
 * directories (Artists, Tracks, ...)
 */
enum mfs_filetype
mfs_get_filetype(const char *path) {
	int tokens = mfs_numtoken(path);

	if (strncmp(path, "/Genres", 7) == 0 ||
	    strncmp(path, "/Artists", 8) == 0) {
		switch (tokens) {
		case 1:
		case 2:
		case 3:
			return (MFS_DIRECTORY);
		case 4:
			return (MFS_FILE);
		}
	} else if (strncmp(path, "/Albums", 7) == 0) {
		switch (tokens) {
		case 1:
		case 2:
			return (MFS_DIRECTORY);
		case 3:
			return (MFS_FILE);
		}
	} else if (strncmp(path, "/Tracks", 7) == 0) {
		switch (tokens) {
		case 1:
			return (MFS_DIRECTORY);
		case 2:
			return (MFS_FILE);
		}
	}

	return (-1);
}
