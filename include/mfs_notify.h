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

#ifndef _MFS_NOTIFY_H_
#define _MFS_NOTIFY_H_

#include <sys/types.h>

#define EVENT_DELETE	0x01
#define EVENT_WRITE	0x02
#define EVENT_EXTEND	0x04
#define EVENT_ATTRIB	0x08
#define EVENT_LINK	0x10
#define EVENT_RENAME	0x20
#define EVENT_REVOKE	0x40

/*
 * An notification event that signals something happened.
 */
struct mfs_notify_event {
	uint8_t ev_type;			/* Event type.     */
	struct mfs_notify_entry *ev_data;	/* Affected entry. */
};

/* Callback function for handling events. */
typedef void mfs_callback_fn_t(struct mfs_notify_event *);

/* Get affected path. */
const char *mfs_notify_path(struct mfs_notify_entry *);

/* Initialize notification system. */
int mfs_notify_init(mfs_callback_fn_t *);

/* Register a file for events. */
int mfs_notify_register(const char *);

/* Unregister notifiaction via file or entry. */
int mfs_notify_unregister_file(const char *);
int mfs_notify_unregister_entry(struct mfs_notify_entry *);

#endif /* !_MFS_NOTIFY_H_ */
