/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

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

#include <sqlite3.h>
#include <pthread.h>

#include <debug.h>
#include <musicfs.h>

void
mfs_delete_from_path(sqlite3 *handle, const char *path) {
	sqlite3_stmt *st;
    int res;
	
	res = sqlite3_prepare_v2(handle, "DELETE FROM song WHERE "
	    "filepath LIKE (?||'%')", -1, &st, NULL);
	if (res != SQLITE_OK) {
		DEBUG("Error preparing statement: %s\n",
		    sqlite3_errmsg(handle));
		return;
	}

	DEBUG("removing songs from '%s'\n", path);
	sqlite3_bind_text(st, 1, path, -1, SQLITE_TRANSIENT);
	res = sqlite3_step(st);
	if (res != SQLITE_DONE) {
		DEBUG("Error deleting '%s' from database: %s\n",
		    path, sqlite3_errmsg(handle));
	}
	sqlite3_finalize(st);
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
	/* sqlite doesn't support deleting from multiple tables at the same time :-( */
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
		mfs_delete_from_path(handle, path);
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
}

