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
#include <mnode.h>
#include <queue.h>
#include <debug.h>

struct queryvector {
	char *artist;
	char *title;
	char *genre;
};

/* Local prototypes. */
static int isduplicate(struct collection *, struct queryvector *, int);
static void free_query_vector(struct queryvector *);
static void fill_query_vector(struct queryvector *, struct mnode *);
static int query_matches(struct queryvector *, struct queryvector *, int);
/* Simple list containing our nodes. */
struct collection allmusic;

void
mp3_initscan(char *musicpath)
{
	LIST_INIT(&allmusic.head);
	traverse_hierarchy(musicpath, mp3_scan, &allmusic);
}

/*
 * Traverse a hierarchy given a directory path. Perform fileoperations on the
 * files in the directory giving data as arguments, as well as recursing on
 * sub-directories.
 */
void
traverse_hierarchy(char *dirpath, traverse_fn_t fileop, struct collection *coll)
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
			traverse_hierarchy(filepath, fileop, coll);
			continue;
		}
		/* If it's a regular file, perform the operation. */
		if (st.st_mode & S_IFREG) {
			fileop(filepath, coll);
		}
	}
	closedir(dirp);
}


/* Scan the music initially. */
void
mp3_scan(char *filepath, struct collection *coll)
{
	struct mnode *mp_new;

	mp_new = malloc(sizeof(struct mnode));
	if (mp_new == NULL)
		err(1, "malloc");
	mp_new->tag = taglib_file_new(filepath);
	mp_new->path = strdup(filepath);
	if (mp_new->path == NULL)
		err(1, "strdup");

	/* Insert node into music list. */
	LIST_INSERT_HEAD(&coll->head, mp_new, coll_next);

#ifdef DEBUGGING
	LIST_FOREACH(mp_new, &coll->head, coll_next) {
		DEBUG("Loaded '%s' into database\n", mp_new->path);
	}

#endif
}

/*
 * Given a selection, fetch out the wanted tag and use filler.
 */
void
mp3_filter(struct collection *selection, int filter, struct filler_data *fd)
{
	TagLib_Tag *tag;
	struct mnode *mp;
	char *field, name[MAXPATHLEN];

	LIST_FOREACH(mp, &selection->head, sel_next) {
	
		tag = taglib_file_tag(mp->tag);
		switch (filter) {
			case FILTER_ARTIST:
			field = taglib_tag_artist(tag);
			break;
			case FILTER_GENRE:
			field = taglib_tag_genre(tag);
			break;
			case FILTER_TITLE:
			field = taglib_tag_title(tag);
			break;
			default:
				err(1, "invalid filter given");
			break;
		}
		if (field == NULL || !strcmp(field, ""))
			continue;
		/* XXX: check if we need to free this or not. */
		strncpy(name, field, sizeof(name));
		fd->filler(fd->buf, name, NULL, 0);
	}
}

/*
 * Perform a query selecting artists given a filter.
 */
struct collection *
mp3_select(int flags, char *artist, char *title, char *genre)
{
	struct mnode *mp;
	struct collection *selection;
	struct queryvector qv, qv2;

	if (flags & SELECT_ARTIST)
		qv.artist = artist;
	if (flags & SELECT_TITLE)
		qv.title = title;
	if (flags & SELECT_GENRE)
		qv.genre = genre;

	/* Initialize selection structure. */
	selection = malloc(sizeof(struct collection));
	if (selection == NULL)
		return (NULL);

	LIST_INIT(&selection->head);
	/* Filter our collection. */

	LIST_FOREACH(mp, &allmusic.head, coll_next) {
		/* First make sure it matches our criteria. */
		fill_query_vector(&qv2, mp);
		if (query_matches(&qv2, &qv, flags) && !isduplicate(selection,
		    &qv2, flags))
			LIST_INSERT_HEAD(&selection->head, mp, sel_next);
		free_query_vector(&qv2);
	}

	return (selection);
}

/*
 * Filter out unique fields.
 */
static int
isduplicate(struct collection *selection, struct queryvector *qv, int flags)
{
	struct mnode *mp2;
	struct queryvector qv_entry;

	LIST_FOREACH(mp2, &selection->head, sel_next) {
		/* Compare to determine if it's a duplicate. */
		fill_query_vector(&qv_entry, mp2);
		if (query_matches(&qv_entry, qv, flags)) {
			free_query_vector(&qv_entry);
			return (1);
		}
		free_query_vector(&qv_entry);
	}
	return (0);
}

/* Fill in a query vector given a node with tags. */
static void
fill_query_vector(struct queryvector *qv, struct mnode *mp)
{
	TagLib_Tag *tag;

	tag = taglib_file_tag(mp->tag);
	qv->artist = strdup(taglib_tag_artist(tag));
	qv->title = strdup(taglib_tag_title(tag));
	qv->genre = strdup(taglib_tag_genre(tag));
	taglib_tag_free_strings();
}

/* Free a query vector. */
static void
free_query_vector(struct queryvector *qv)
{

	if (qv->artist != NULL)
		free(qv->artist);
	if (qv->title != NULL)
		free(qv->title);
	if (qv->genre != NULL)
		free(qv->genre);
}

/* Determine if two query vectors matches. */
static int
query_matches(struct queryvector *qv1, struct queryvector *qv2, int flags)
{

	/* Check if it matches the different fields. */
	if ((flags & SELECT_ARTIST) && qv1->artist != NULL && qv2->artist != NULL) {
		if (strcmp(qv1->artist, qv2->artist))
			return (0);
	} 
	if ((flags & SELECT_TITLE) && qv1->title != NULL && qv2->title != NULL) {
		if (strcmp(qv1->title, qv2->title))
			return (0);
	}
	if ((flags & SELECT_GENRE) && qv1->genre != NULL && qv2->genre != NULL) {
		if (strcmp(qv1->genre, qv2->genre))
			return (0);
	}
	return (1);
}

