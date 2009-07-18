/*
 * Musicfs is a FUSE module implementing a media filesystem in userland.
 * Copyright (C) 2009  Ulf Lilleengen, Kjetil Ørbekk
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

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/param.h>
#include <sys/event.h>
#include <sys/time.h>
#endif

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mfs_notify.h>

/*
 * One entry for the file that should be handled.
 */
struct mfs_notify_entry {
	int fd;
	char path[MAXPATHLEN + 1];
	LIST_ENTRY(mfs_notify_entry) next;
};

/*
 * Data structure to keep track of all files and directories we want to keep
 * track of.
 */
struct mfs_notify_list {
	LIST_HEAD(, mfs_notify_entry) nl_head;
	pthread_mutex_t nl_lock;
#define MFS_NOTIFYLIST_LOCK(l) pthread_mutex_lock(&(l)->nl_lock)
#define MFS_NOTIFYLIST_UNLOCK(l) pthread_mutex_unlock(&(l)->nl_lock)

#if defined(__FreeBSD__)
	int kqueue_fd;
#endif
	mfs_callback_fn_t *handler;
};

/* XXX: We use a global list for now. */
struct mfs_notify_list nl;

#if defined(__FreeBSD__)
pthread_t mfs_notify_kqueue_thr;
static void *mfs_notify_kqueue_handler(void *);
#endif

/* Initialize the notification system. */
int
mfs_notify_init(mfs_callback_fn_t *fn)
{
	assert(fn != NULL);

	LIST_INIT(&nl.nl_head);
	pthread_mutex_init(&nl.nl_lock, NULL);
	nl.handler = fn;

#if defined(__FreeBSD__)
	nl.kqueue_fd = kqueue();
#endif
	return (0);
}

/* Register a file for events. */
int
mfs_notify_register(const char *path)
{
	struct mfs_notify_entry *ent;
	
	assert(path != NULL);
	ent = malloc(sizeof(struct mfs_notify_entry));
	if (ent == NULL)
		return (-1);
	strlcpy(ent->path, path, sizeof(ent->path));
	ent->fd = open(path, O_RDONLY);
	if (ent->fd < 0)
		return (-1);
	MFS_NOTIFYLIST_LOCK(&nl);
	LIST_INSERT_HEAD(&nl.nl_head, ent, next);
	MFS_NOTIFYLIST_UNLOCK(&nl);

#if defined(__FreeBSD__)
	struct kevent ev;
	EV_SET(&ev, ent->fd, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_CLEAR,
	    NOTE_RENAME | NOTE_WRITE | NOTE_DELETE, 0, ent);
	kevent(nl.kqueue_fd, &ev, 1, NULL, 0, NULL);
	pthread_create(&mfs_notify_kqueue_thr, NULL, mfs_notify_kqueue_handler,
	    &nl);
#endif
	return (0);
}

/* Unregister a file for events. */
int
mfs_notify_unregister_file(const char *path)
{
	struct mfs_notify_entry *ent;

	MFS_NOTIFYLIST_LOCK(&nl);
	LIST_FOREACH(ent, &nl.nl_head, next) {
		if (strcmp(ent->path, path) == 0) {
			LIST_REMOVE(ent, next);
			close(ent->fd);
			free(ent);
			break;
		}
	}
	MFS_NOTIFYLIST_UNLOCK(&nl);
	return (0);
}

/* Unregister a file for events using the entry directly. */
int
mfs_notify_unregister_entry(struct mfs_notify_entry *ent)
{
	return (mfs_notify_unregister_file(ent->path));
}

/* Return path associated with entry. */
const char *
mfs_notify_path(struct mfs_notify_entry *ent)
{
	return (ent->path);
}

#if defined(__FreeBSD__)
static void *
mfs_notify_kqueue_handler(void *arg)
{
	struct mfs_notify_list *nlp = (struct mfs_notify_list *)arg;
	struct kevent ev;
	int n;

	assert(nlp != NULL);

	for (;;) {
		n = kevent(nlp->kqueue_fd, NULL, 0, &ev, 1, NULL);
		if (n <= 0)
			continue;
		struct mfs_notify_entry *ent = ev.udata;
		assert(ent != NULL);

		struct mfs_notify_event e;
		e.ev_type = 0;
		/* Convert to system independent flags. */
		if (ev.fflags & NOTE_DELETE)
			e.ev_type |= EVENT_DELETE;
		else if (ev.fflags & NOTE_WRITE)
			e.ev_type |= EVENT_WRITE;
		else if (ev.fflags & NOTE_EXTEND)
			e.ev_type |= EVENT_EXTEND;
		else if (ev.fflags & NOTE_ATTRIB)
			e.ev_type |= EVENT_ATTRIB;
		else if (ev.fflags & NOTE_LINK)
			e.ev_type |= EVENT_LINK;
		else if (ev.fflags & NOTE_RENAME)
			e.ev_type |= EVENT_RENAME;
		else if (ev.fflags & NOTE_REVOKE)
			e.ev_type |= EVENT_REVOKE;

		e.ev_data = ent;
		nlp->handler(&e);
	}
}
#endif
