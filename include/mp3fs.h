#ifndef _MP3FS_H_
#define _MP3FS_H_
struct fuse_args;
struct collection;
struct mnode;

int	mp3_run(int, char **);
struct collection *mp3_initscan(void);
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
#endif
