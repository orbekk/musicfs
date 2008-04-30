#ifndef _MP3FS_H_
#define _MP3FS_H_
struct fuse_args;
struct collection;
struct mnode;

int	mp3_run(int, char **);
void	mp3_initscan(char *);
/*
 * Data passed to traverse function pointers.'
 */
struct filler_data {
	void *buf;
	fuse_fill_dir_t filler;
};

/* 
 * Functions traversing the underlying filesystem and do operations on the
 * files, for instance scanning the collection.
 */
typedef void traverse_fn_t(char *, struct collection *);
void traverse_hierarchy(char *, traverse_fn_t, struct collection *);
traverse_fn_t mp3_scan;

/*
 * Functions performing queries on the collection.
 * Usage:
 * To list all artists:
 	mp3_select(SELECT_ARTIST, NULL, NULL, NULL);
 * To list one artist "Haddaway":
 	mp3_select(SELECT_ARTIST, "Haddaway", NULL, NULL);
 * To list unique artists in genre "Rock"
 	mp3_select(SELECT_ARTIST | SELECT_GENRE, NULL, NULL, "Rock");

 * So, one can say that the first parameter specifies which fields should be
 * taken into account when comparing for duplicates, and the field parameters
 * determines if * there are some fields of that paramters which it must match.

 * I hope to improve this soon.
 */
#define SELECT_ARTIST 0x1
#define SELECT_TITLE 0x2
#define SELECT_GENRE 0x4
struct collection *mp3_select(int, char *, char *, char *);

/*
 * Use a selection to input a certain tag field into a filler.
 */
/* XXX: no need to use bitmask since we only need one field at a time? */
#define FILTER_ARTIST 0
#define FILTER_GENRE 1
#define FILTER_TITLE 2
void	mp3_filter(struct collection *, int, struct filler_data *);
#endif
