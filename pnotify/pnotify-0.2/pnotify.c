/*		$Id: $		*/

/*
 * Copyright (c) 2007 Mark Heily <devel@heily.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include "pnotify.h"
#include "queue.h"

#if defined(__linux__)
# define HAVE_INOTIFY 1
# include <sys/inotify.h>
#else
# define HAVE_KQUEUE 1
# include <sys/event.h>
#endif

/* The 'dprintf' macro is used for printf() debugging */
#if PNOTIFY_DEBUG
# define dprintf printf
# define dprint_event(e) (void) pnotify_print_event(e)
#else
# define dprintf(...)    do { ; } while (0)
# define dprint_event(e) do { ; } while (0)
#endif

/** The maximum number of watches that a single controller can have */
static const int WATCH_MAX = 20000;

/** An entire directory that is under watch */
struct directory {

	/** The full pathname of the directory */
	char    *path;
	size_t   path_len;

	/** A directory handle returned by opendir(3) */
	DIR     *dirp;

	/* All entries in the directory (struct dirent) */
	LIST_HEAD(, dentry) all;
};

/** A directory entry list element */
struct dentry {

	/** The directory entry from readdir(3) */
	struct dirent   ent;

	/** A file descriptor used by kqueue(4) to monitor the entry */
	int  fd;

	/** The file status from stat(2) */
	struct stat     st;

	/** All event(s) that occurred on this file */
	int mask;

	/** Pointer to the next directory entry */
        LIST_ENTRY(dentry) entries;
};


/** An internal watch entry */
struct pnotify_watch {
	int             fd;	/**< kqueue(4) watched file descriptor */
	int             wd;	/**< Unique 'watch descriptor' */
	char            path[PATH_MAX + 1];	/**< Path associated with fd */
	uint32_t        mask;	/**< Mask of monitored events */

	bool            is_dir;	/**< TRUE, if the file is a directory */

	/* A list of directory entries (only used if is_dir is true) */
	struct directory dir;

	/* The parent watch descriptor, for watches that are
	   automatically added on files within a monitored
	   directory. If zero, there is no parent.
	 */
	int parent_wd;

#if HAVE_KQUEUE

	/* The associated kernel event structure */
	struct kevent    kev;

#endif

	/* Pointer to the next watch */
	LIST_ENTRY(pnotify_watch) entries;
};

/* Forward declarations */
#if HAVE_KQUEUE
static int directory_scan(struct pnotify_cb *ctl, struct pnotify_watch * watch);
static int directory_open(struct pnotify_cb *ctl, struct pnotify_watch * watch);
static int kq_directory_event_handler(struct kevent kev, struct pnotify_cb * ctl, struct pnotify_watch * watch);

#  define sys_create_event_queue	kqueue
#  define sys_add_watch			kq_add_watch
#  define sys_rm_watch			kq_rm_watch
#  define sys_get_event			kq_get_event
#elif HAVE_INOTIFY
#  define sys_create_event_queue	inotify_init
#  define sys_add_watch			in_add_watch
#  define sys_rm_watch			in_rm_watch
#  define sys_get_event			in_get_event
#endif


#if HAVE_KQUEUE

static inline int
kq_add_watch(struct pnotify_cb *ctl, struct pnotify_watch *watch)
{
	struct stat     st;
	struct kevent *kev = &watch->kev;
	int mask = watch->mask;

	/* Open the file */
	if ((watch->fd = open(watch->path, O_RDONLY)) < 0) {
		warn("opening path `%s' failed", watch->path);
		return -1;
	}

	/* Test if the file is a directory */
	if (fstat(watch->fd, &st) < 0) {
		warn("fstat(2) failed");
		return -1;
	}
	watch->is_dir = S_ISDIR(st.st_mode);

	/* Initialize the directory structure, if needed */
	if (watch->is_dir) 
		directory_open(ctl, watch);

	/* Generate a new watch ID */
	/* FIXME - this never decreases and might fail */
	if ((watch->wd = ++ctl->next_wd) > WATCH_MAX) {
		warn("watch_max exceeded");
		return -1;
	}

	/* Create and populate a kevent structure */
	EV_SET(kev, watch->fd, EVFILT_VNODE, EV_ADD | EV_CLEAR, 0, 0, watch);
	if (mask & PN_ACCESS || mask & PN_MODIFY)
		kev->fflags |= NOTE_ATTRIB;
	if (mask & PN_CREATE)
		kev->fflags |= NOTE_WRITE;
	if (mask & PN_DELETE)
		kev->fflags |= NOTE_DELETE | NOTE_WRITE;
	if (mask & PN_MODIFY)
		kev->fflags |= NOTE_WRITE | NOTE_TRUNCATE | NOTE_EXTEND;
	if (mask & PN_ONESHOT)
		kev->flags |= EV_ONESHOT;

	/* Add the kevent to the kernel event queue */
	if (kevent(ctl->fd, kev, 1, NULL, 0, NULL) < 0) {
		perror("kevent(2)");
		return -1;
	}

	return 0;
}


static inline int
kq_rm_watch(struct pnotify_cb *ctl, struct pnotify_watch *watch)
{
	/* Close the file descriptor.
	  The kernel will automatically delete the kevent 
	  and any pending events.
	 */
	if (close(watch->fd) < 0) {
		perror("unable to close watch fd");
		return -1;
	}

	/* Close the directory handle */
	if (watch->dir.dirp != NULL) {
		if (closedir(watch->dir.dirp) != 0) {
			perror("closedir(3)");
			return -1;
		}
	}

	return 0;
}

static inline int
kq_get_event(struct pnotify_event *evt, struct pnotify_cb *ctl)
{
	struct pnotify_watch *watch;
	struct pnotify_event *evp;
	struct kevent   kev;
	int rc;

retry:

	/* Wait for an event notification from the kernel */
	dprintf("waiting for kernel event..\n");
	rc = kevent(ctl->fd, NULL, 0, &kev, 1, NULL);
	if ((rc < 0) || (kev.flags & EV_ERROR)) {
		warn("kevent(2) failed");
		return -1;
	}

	/* Find the matching watch structure */
	watch = (struct pnotify_watch *) kev.udata;

	/* Workaround:

	   Deleting a file in a watched directory causes two events:
	     	NOTE_MODIFY on the directory
		NOTE_DELETE on the file

	   We ignore the NOTE_DELETE on the file.
        */
	if (watch->parent_wd && kev.fflags & NOTE_DELETE) {
		dprintf("ignoring NOTE_DELETE on a watched file\n");
		goto retry;
	}

	/* Convert the kqueue(4) flags to pnotify_event flags */
	if (!watch->is_dir) {
		if (kev.fflags & NOTE_WRITE)
			evt->mask |= PN_MODIFY;
		if (kev.fflags & NOTE_TRUNCATE)
			evt->mask |= PN_MODIFY;
		if (kev.fflags & NOTE_EXTEND)
			evt->mask |= PN_MODIFY;
#if TODO
		// not implemented yet
		if (kev.fflags & NOTE_ATTRIB)
			evt->mask |= PN_ATTRIB;
#endif
		if (kev.fflags & NOTE_DELETE)
			evt->mask |= PN_DELETE;

		/* Construct a pnotify_event structure */
		if ((evp = calloc(1, sizeof(*evp))) == NULL) {
			warn("malloc failed");
			return -1;
		}

		/* If the event happened within a watched directory,
		   add the filename and the parent watch descriptor.
		*/
		if (watch->parent_wd) {

			/* KLUDGE: remove the leading basename */
			char *fn = strrchr(watch->path, '/') ;
			if (!fn) { 
				fn = watch->path;
			} else {
				fn++;
			}

			evt->wd = watch->parent_wd;
			/* FIXME: more error handling */
			(void) strncpy(evt->name, fn, strlen(fn));
		}

		/* Add the event to the list of pending events */
		memcpy(evp, evt, sizeof(*evp));
		STAILQ_INSERT_TAIL(&ctl->event, evp, entries);

		dprint_event(evt);

	/* Handle events on directories */
	} else {

		/* When a file is added or deleted, NOTE_WRITE is set */
		if (kev.fflags & NOTE_WRITE) {
			if (kq_directory_event_handler(kev, ctl, watch) < 0) {
				warn("error processing diretory");
				return -1;
			}
		}
		/* FIXME: Handle the deletion of a watched directory */
		else if (kev.fflags & NOTE_DELETE) {
			warn("unimplemented - TODO");
			return -1;
		} else {
			warn("unknown event recieved");
			return -1;
		}

	}

	return 0;
}

#endif /* HAVE_KQUEUE */

/* ------------------------ inotify(7) wrappers -------------------- */

#if HAVE_INOTIFY

static inline int
in_add_watch(struct pnotify_cb *ctl, struct pnotify_watch *watch)
{
	int mask = watch->mask;
	uint32_t        imask = 0;

	/* Generate the mask */
	if (mask & PN_ACCESS || mask & PN_MODIFY)
		imask |= IN_ACCESS | IN_ATTRIB;
	if (mask & PN_CREATE)
		imask |= IN_CREATE;
	if (mask & PN_DELETE)
		imask |= IN_DELETE | IN_DELETE_SELF;
	if (mask & PN_MODIFY)
		imask |= IN_MODIFY;
	if (mask & PN_ONESHOT)
		imask |= IN_ONESHOT;

	/* Add the event to the kernel event queue */
	watch->wd = inotify_add_watch(ctl->fd, watch->path, imask);
	if (watch->wd < 0) {
		perror("inotify_add_watch(2) failed");
		return -1;
	}

	return 0;
}

static inline int
in_rm_watch(struct pnotify_cb *ctl, struct pnotify_watch *watch)
{
	// FIXME - this causes an IN_IGNORE event to be generated;
        // 	need to handle
	if (inotify_rm_watch(ctl->fd, watch->wd) < 0) {
		perror("inotify_rm_watch(2)");
		return -1;
	}

	return 0;
}

static inline int
in_get_event(struct pnotify_event *evt, struct pnotify_cb *ctl)
{
	struct pnotify_watch *watch;
	struct pnotify_event *evp;
	struct inotify_event *iev, *endp;
	ssize_t         bytes;
	char            buf[4096];

retry:

	/* Read the event structure */
	bytes = read(ctl->fd, &buf, sizeof(buf));
	if (bytes <= 0) {
		if (errno == EINTR)
			goto retry;
		else
			perror("read(2)");

		return -1;
	}

	/* Compute the beginning and end of the event list */
	iev = (struct inotify_event *) & buf;
	endp = iev + bytes;

	/* Process each pending event */
	while (iev < endp) {

		/* XXX-WORKAROUND */
		if (iev->wd == 0)
			break;

		/* Find the matching watch structure */
		LIST_FOREACH(watch, &ctl->watch, entries) {
			if (watch->wd == iev->wd)
				break;
		}
		if (!watch) {
			warn("watch # %d not found", iev->wd);
			return -1;
		}
		/* Construct a pnotify_event structure */
		if ((evp = calloc(1, sizeof(*evp))) == NULL) {
			warn("malloc failed");
			return -1;
		}
		evp->wd = watch->wd;
		(void) strncpy(evp->name, iev->name, iev->len);
		if (iev->mask & IN_ACCESS)
			evp->mask |= PN_ACCESS;
#if TODO
		// not implemented
		if (iev->mask & IN_ATTRIB)
			evp->mask |= PN_ATTRIB;
#endif
		if (iev->mask & IN_MODIFY)
			evp->mask |= PN_MODIFY;
		if (iev->mask & IN_CREATE)
			evp->mask |= PN_CREATE;
		if (iev->mask & IN_DELETE)
			evp->mask |= PN_DELETE;
		if (iev->mask & IN_DELETE_SELF) {
			evp->mask |= PN_DELETE;
			(void) strncpy(evp->name, "", 0);
		}

		/* Add the event to the list of pending events */
		STAILQ_INSERT_TAIL(&ctl->event, evp, entries);

		/* Go to the next event */
		iev += sizeof(*iev) + iev->len;
	}

	return 0;
}

#endif /* HAVE_INOTIFY */

/* ----------------------- pnotify(3) interface -------------------- */

int
pnotify_init(struct pnotify_cb *ctl)
{
	assert(ctl != NULL);

	memset(ctl, 0, sizeof(*ctl));

	/* Create a kernel event queue */
	ctl->fd = sys_create_event_queue();
	if (ctl->fd < 0) {
		warn("unable to create event descriptor");
		return -1;
	}

	LIST_INIT(&ctl->watch);
	STAILQ_INIT(&ctl->event);

	return 0;
}


int
pnotify_add_watch(struct pnotify_cb *ctl, const char *pathname, int mask)
{
	struct pnotify_watch *watch;

	assert(ctl && pathname);

	/* Allocate a new entry */
	if ((watch = malloc(sizeof(*watch))) == NULL) {
		warn("malloc error");
		return -1;
	}
	(void) strncpy((char *) &watch->path, pathname, sizeof(watch->path));	
	watch->mask = mask;

	/* Register the watch with the kernel */
	if (sys_add_watch(ctl, watch) < 0) {
		warn("adding watch failed");
		free(watch);
		return -1;
	}

	/* Update the control block */
	LIST_INSERT_HEAD(&ctl->watch, watch, entries);

	dprintf("added watch: wd=%d mask=%d path=%s\n", 
		watch->wd, watch->mask, watch->path);

	return watch->wd;
}


/**
  Remove a watch.

  @param ctl a pnotify control block
  @param wd FIXME --- WONT WORK
  @return 0 if successful, or -1 if an error occurred.
*/
int 
pnotify_rm_watch(struct pnotify_cb * ctl, int wd)
{
	struct pnotify_watch *watchp, *wtmp;
	int found = 0;

	assert(ctl && wd >= 0);

	/* Find the matching watch structure(s) */
	LIST_FOREACH_SAFE(watchp, &ctl->watch, entries, wtmp) {

		/* Remove the parent watch and it's children */
		if ((watchp->wd == wd) || (watchp->parent_wd == wd)) {
			if (sys_rm_watch(ctl, watchp) < 0) 
				return -1;
			LIST_REMOVE(watchp, entries);
			free(watchp);
			found++;
		}
	}

	if (found == 0) {
		warn("watch # %d not found", wd);
		return -1;
	} else {
		return 0;
	}
}


int
pnotify_get_event(struct pnotify_event * evt,
		  struct pnotify_cb * ctl
		 )
{
	struct pnotify_event *evp;

	assert(evt && ctl);

	/* If there are pending events in the queue, return the first one */
	if (!STAILQ_EMPTY(&ctl->event)) 
		goto next_event;

retry_wait:

	/* Reset the event structure to be empty */
	memset(evt, 0, sizeof(*evt));

	/* Wait for a kernel event */
	if (sys_get_event(evt, ctl) < 0) 
		return -1;

next_event:

	/* Shift the first element off of the pending event queue */
	if (!STAILQ_EMPTY(&ctl->event)) {
		evp = STAILQ_FIRST(&ctl->event);
		STAILQ_REMOVE_HEAD(&ctl->event, entries);
		memcpy(evt, evp, sizeof(*evt));
		free(evp);

		/* If the event mask is zero, get the next event */
		if (evt->mask == 0)
			goto next_event;
	} else {
		//warn("spurious wakeup");
		goto retry_wait;
	}

	return 0;
}


int
pnotify_print_event(struct pnotify_event * evt)
{
	assert(evt);

	return printf("event: wd=%d mask=(%s%s%s%s%s) name=`%s'\n",
		      evt->wd,
		      evt->mask & PN_ACCESS ? "access," : "",
		      evt->mask & PN_CREATE ? "create," : "",
		      evt->mask & PN_DELETE ? "delete," : "",
		      evt->mask & PN_MODIFY ? "modify," : "",
		      evt->mask & PN_ERROR ? "error," : "",
		      evt->name);
}


void
pnotify_dump(struct pnotify_cb *ctl)
{
	struct pnotify_event *evt;

	printf("\npending events:\n");
	STAILQ_FOREACH(evt, &ctl->event, entries) {
		printf("\t");
		(void) pnotify_print_event(evt);
	}
	printf("/* end: pending events */\n");
}


int
pnotify_free(struct pnotify_cb *ctl)
{
	struct pnotify_watch *watch;
	struct pnotify_event *evt, *nxt;

	assert(ctl != NULL);

	/* Delete all watches */
	while (!LIST_EMPTY(&ctl->watch)) {
		watch = LIST_FIRST(&ctl->watch);
		if (pnotify_rm_watch(ctl, watch->wd) < 0) {
			warn("error removing watch");
			return -1;
		}
	}

	/* Delete all pending events */
  	evt = STAILQ_FIRST(&ctl->event);
        while (evt != NULL) {
		nxt = STAILQ_NEXT(evt, entries);
		free(evt);
		evt = nxt;
    	}

	/* Close the event descriptor */
	if (close(ctl->fd) < 0) {
		perror("close(2)");
		return -1;
	}

	return 0;
}


/*-------------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------------*/

#if HAVE_KQUEUE

/**
 Open a watched directory.
*/
static int
directory_open(struct pnotify_cb *ctl, struct pnotify_watch * watch)
{
	struct directory * dir;

	dir = &watch->dir;

	/* Initialize the li_directory structure */
	LIST_INIT(&dir->all);
	if ((dir->dirp = opendir(watch->path)) == NULL) {
		perror("opendir(2)");
		return -1;
	}

	/* Store the pathname */
	dir->path_len = strlen(watch->path);
	if ((dir->path_len >= PATH_MAX) || 
		((dir->path = malloc(dir->path_len + 1)) == NULL)) {
			perror("malloc(3)");
			return -1;
	}
	strncpy(dir->path, watch->path, dir->path_len);
		
	/* Scan the directory */
	if (directory_scan(ctl, watch) < 0) {
		warn("directory_scan failed");
		return -1;
	}

	return 0;
}


/**
 Scan a directory looking for new, modified, and deleted files.
 */
static int
directory_scan(struct pnotify_cb *ctl, struct pnotify_watch * watch)
{
	struct pnotify_watch *wtmp;
	struct directory *dir;
	struct dirent   ent, *entp;
	struct dentry  *dptr;
	bool            found;
	char            path[PATH_MAX + 1];
	char           *cp;
	struct stat	st;

	assert(watch != NULL);

	dir = &watch->dir;

	dprintf("scanning directory\n");

	/* Generate the basename */
	(void) snprintf((char *) &path, PATH_MAX, "%s/", dir->path);
	cp = path + dir->path_len + 1;

	/* 
	 * Invalidate the status mask for all entries.
	 *  This is used to detect entries that have been deleted.
 	 */
	LIST_FOREACH(dptr, &dir->all, entries) {
		dptr->mask = PN_DELETE;
	}

	/* Skip the initial '.' and '..' entries */
	/* XXX-FIXME doesnt work with chroot(2) when '..' doesn't exist */
	rewinddir(dir->dirp);
	if (readdir_r(dir->dirp, &ent, &entp) != 0) {
		perror("readdir_r(3)");
		return -1;
	}
	if (strcmp(ent.d_name, ".") == 0) {
		if (readdir_r(dir->dirp, &ent, &entp) != 0) {
			perror("readdir_r(3)");
			return -1;
		}
	}

	/* Read all entries in the directory */
	for (;;) {

		/* Read the next entry */
		if (readdir_r(dir->dirp, &ent, &entp) != 0) {
			perror("readdir_r(3)");
			return -1;
		}

		/* Check for the end-of-directory condition */
		if (entp == NULL)
			break;
 
		/* Perform a linear search for the dentry */
		found = false;
		LIST_FOREACH(dptr, &dir->all, entries) {

			/*
			 * BUG - this doesnt handle hardlinks which
			 * have the same d_fileno but different
			 * dirent structs
			 */
			//should compare the entire dirent struct...
			if (dptr->ent.d_fileno != ent.d_fileno) 
				continue;

			dprintf("old entry: %s\n", ent.d_name);
			dptr->mask = 0;

			/* Generate the full pathname */
			// BUG: name_max is not precise enough
			strncpy(cp, ent.d_name, NAME_MAX);

			/* Check the mtime and atime */
			if (stat((char *) &path, &st) < 0) {
				perror("stat(2)");
				return -1;
			}

			found = true;
			break;
		}

		/* Add the entry to the list, if needed */
		if (!found) {
			dprintf("new entry: %s\n", ent.d_name);

			/* Allocate a new dentry structure */
			if ((dptr = malloc(sizeof(*dptr))) == NULL) {
				perror("malloc(3)");
				return -1;
			}

			/* Copy the dirent structure */
			memcpy(&dptr->ent, &ent, sizeof(ent));
			dptr->mask = PN_CREATE;

			/* Generate the full pathname */
			// BUG: name_max is not precise enough
			strncpy(cp, ent.d_name, NAME_MAX);

			/* Get the file status */
			if (stat((char *) &path, &st) < 0) {
				perror("stat(2)");
				return -1;
			}

			/* Add a watch if it is a regular file */
			if (S_ISREG(st.st_mode)) {
				if (pnotify_add_watch(ctl, path, 
					watch->mask) < 0)
						return -1;
				wtmp = LIST_FIRST(&ctl->watch);
				wtmp->parent_wd = watch->wd;
			}

			LIST_INSERT_HEAD(&dir->all, dptr, entries);
		}
	}

	return 0;
}



/**
 Handle an event inside a directory (kqueue version)
*/
static int
kq_directory_event_handler(struct kevent kev,
			   struct pnotify_cb * ctl,
			   struct pnotify_watch * watch)
{
	struct pnotify_event *ev;
	struct dentry  *dptr, *dtmp;

	assert(ctl && watch);

	/* Re-scan the directory to find new and deleted files */
	if (directory_scan(ctl, watch) < 0) {
		warn("directory_scan failed");
		return -1;
	}

	/* Generate an event for each changed directory entry */
	LIST_FOREACH_SAFE(dptr, &watch->dir.all, entries, dtmp) {

		/* Skip files that have not changed */
		if (dptr->mask == 0)
			continue;

		/* Construct a pnotify_event structure */
		if ((ev = calloc(1, sizeof(*ev))) == NULL) {
			warn("malloc failed");
			return -1;
		}
		ev->wd = watch->wd;
		ev->mask = dptr->mask;
		(void) strlcpy(ev->name, dptr->ent.d_name, sizeof(ev->name));
		dprint_event(ev);

		/* Add the event to the list of pending events */
		STAILQ_INSERT_TAIL(&ctl->event, ev, entries);

		/* Remove the directory entry for a deleted file */
		if (dptr->mask & PN_DELETE) {
			LIST_REMOVE(dptr, entries);
			free(dptr);
		}
	}

	return 0;
}
#endif
