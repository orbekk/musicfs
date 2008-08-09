#ifndef _MP3FS_H_
#define _MP3FS_H_
struct fuse_args;

#define DBNAME "/home/lulf/dev/mp3fs/music.db"

int	mp3_run(int, char **);
int	mp3_initscan(char *);


/* 
 * Functions traversing the underlying filesystem and do operations on the
 * files, for instance scanning the collection.
 */
typedef void traverse_fn_t(char *);
void traverse_hierarchy(char *, traverse_fn_t);
traverse_fn_t mp3_scan;

/*
 * Data passed to mp3_list.
 */
struct filler_data {
	void *buf;
	fuse_fill_dir_t filler;
};

#define LIST_DATATYPE_STRING 1
#define LIST_DATATYPE_INT 2

struct listhandle;
struct listhandle	*mp3_list_start(int, struct filler_data *, const char *);
void			 mp3_list_insert(struct listhandle *, void *, int);
void			 mp3_list_finish(struct listhandle *);

void	 mp3_list_artist(const char *, struct filler_data *);
void	 mp3_list_genre(const char *, struct filler_data *);
char	*mp3_gettoken(const char *, int);
int	 mp3_numtoken(const char *);
#endif
