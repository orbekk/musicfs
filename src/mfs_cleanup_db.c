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

#include <fusever.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include <pthread.h>

#include <debug.h>
#include <musicfs.h>

int
execute_statement(sqlite3 *handle, const char *query,
	       const char *fields[])
{
	sqlite3_stmt *st;
	int res;
	
	res = sqlite3_prepare_v2(handle, query, -1, &st, NULL);
	if (res != SQLITE_OK) {
		DEBUG("Error preparing statement: %s\n",
		    sqlite3_errmsg(handle));
		return (-1);
	}
	
	for (int i=0; fields[i] != NULL; i++) {
		sqlite3_bind_text(st, i+1, fields[i], -1, SQLITE_TRANSIENT);
	}
	res = sqlite3_step(st);
	if (res != SQLITE_DONE) {
		DEBUG("Error executing query %s\n\t %s\n",
		    query, sqlite3_errmsg(handle));
		return (-1);
	}
	sqlite3_finalize(st);
	return (0);
}

void
delete_from_path(sqlite3 *handle, const char *path)
{
	const char *fields[] = {path, NULL};

	execute_statement(handle,
	    "DELETE FROM song WHERE filepath LIKE (?||'%')",
	    fields);
}

void
delete_artist(sqlite3 *handle, const char *artist)
{
	const char *fields[] = {artist, NULL};
	execute_statement(handle,
	    "DELETE FROM artist WHERE name == ?",
	    fields);
}

void
delete_genre(sqlite3 *handle, const char *genre)
{
	const char *fields[] = {genre, NULL};
	execute_statement(handle,
	    "DELETE FROM genre WHERE name == ?",
	    fields);
}

void
cleanup_artists(sqlite3 *handle)
{
	sqlite3_stmt *st;
	int res;
	const char *artist, *song_title;

	/* This is a slow, but probably faster than doing a lot of
	   queries */
	res = sqlite3_prepare_v2(handle, "SELECT a.name, s.title "
	    "FROM artist AS a LEFT JOIN song AS s ON "
	    "(a.name==s.artistname) GROUP BY a.name",
	    -1, &st, NULL);
	
	if (res != SQLITE_OK) {
		DEBUG("Error preparing statement: %s\n",
		    sqlite3_errmsg(handle));
		return;
	}
	
	res = sqlite3_step(st);
	while (res == SQLITE_ROW) {
		artist = (const char*)sqlite3_column_text(st, 0);
		song_title = (const char*)sqlite3_column_text(st, 1);
		
		if (song_title == NULL || strcmp(song_title, "") == 0) {
			/* No song by this artist */
			delete_artist(handle, artist);
		}
		
		res = sqlite3_step(st);
	}
}

void
cleanup_genres(sqlite3 *handle)
{
	/* pretty much the same as cleanup_artist, only for genres */
	sqlite3_stmt *st;
	int res;
	const char *genre, *song_title;

	res = sqlite3_prepare_v2(handle, "SELECT g.name, s.title "
	    "FROM genre AS g LEFT JOIN song AS s ON "
	    "(g.name==s.genrename) GROUP BY g.name",
	    -1, &st, NULL);
	
	if (res != SQLITE_OK) {
		DEBUG("Error preparing statement: %s\n",
		    sqlite3_errmsg(handle));
		return;
	}
	
	res = sqlite3_step(st);
	while (res == SQLITE_ROW) {
		genre = (const char*)sqlite3_column_text(st, 0);
		song_title = (const char*)sqlite3_column_text(st, 1);
		
		if (song_title == NULL || strcmp(song_title, "") == 0) {
			/* No song with this genre */
			delete_genre(handle, genre);
		}
		
		res = sqlite3_step(st);
	}
}

/*
 * Clean up the database:
 *
 * - Remove disabled paths
 * - Remove songs in disabled paths
 * - Remove unused artists and genres
 */
void
mfs_cleanup_db(sqlite3 *handle)
{
	DEBUG("cleaning up db\n");
	/* sqlite doesn't support deleting from multiple tables at the
	   same time :-( */
	sqlite3_stmt *st;
	int res;
	const char *path;

	
	/* Removed songs from inactive paths */
	res = sqlite3_prepare_v2(handle, "SELECT path FROM path WHERE "
	    "active = 0", -1, &st, NULL);
	if (res != SQLITE_OK) {
		DEBUG("Error preparing statement: %s\n",
		    sqlite3_errmsg(handle));
		return;
	}
	res = sqlite3_step(st);
	
	while (res == SQLITE_ROW) {
		path = (const char*)sqlite3_column_text(st, 0);
		delete_from_path(handle, path);
		res = sqlite3_step(st);
	}
	sqlite3_finalize(st);
	
	/* Remove the actual paths */
	res = sqlite3_prepare_v2(handle, "DELETE FROM path WHERE "
	    "active = 0", -1, &st, NULL);
	if (res != SQLITE_OK) {
		DEBUG("Error preparing statement: %s\n",
		    sqlite3_errmsg(handle));
		return;
	}
	res = sqlite3_step(st);
	if (res != SQLITE_DONE) {
		DEBUG("Error deleting inactive paths from db: %s\n",
		    sqlite3_errmsg(handle));
		return;
	}
	sqlite3_finalize(st);

	/* These are a bit heavy :-( */
	cleanup_artists(handle);
	cleanup_genres(handle);
}

