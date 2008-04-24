#ifndef _MP3FS_H_
#define _MP3FS_H_
struct fuse_args;
int	mp3_run(int, char **);


/*
 * Data passed to traverse function pointers.'
 */
struct filler_data {
	void *buf;
	fuse_fill_dir_t filler;
};
/* Traverse files used in mp3_subr.c */
typedef void traverse_fn_t(char *, void *);
void traverse_hierarchy(char *, traverse_fn_t, void *);

traverse_fn_t mp3_artist;
#endif
