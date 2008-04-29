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
 */
struct collection *mp3_select(char *, char *, char *);

/*
 * Use a selection to input a certain tag field into a filler.
 */
/* XXX: no need to use bitmask since we only need one field at a time? */
#define FILTER_ARTIST 0
#define FILTER_GENRE 1
#define FILTER_TITLE 2
void	mp3_filter(struct collection *, int, struct filler_data *);
#endif
