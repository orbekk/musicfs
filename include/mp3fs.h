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
 * Data passed to mp3_lookup_list
 */
struct filler_data {
	void *buf;
	fuse_fill_dir_t filler;
};

/*
 * Data passed to mp3_lookup_open
 */
struct file_data {
	int fd;
	int found;
};

#define LIST_DATATYPE_STRING 1
#define LIST_DATATYPE_INT 2

enum mp3_filetype { MP3_NOTFOUND, MP3_FILE, MP3_DIRECTORY };

/* A lookup functions returns 0 if it's finished. */
typedef int lookup_fn_t(void *, const char *);
/* Lookup function listing each row. */
lookup_fn_t mp3_lookup_list;
/* Lookup function opening the first file match. */
lookup_fn_t mp3_lookup_open;
/* Lookup function stat'ing a file. */
lookup_fn_t mp3_lookup_stat;

struct lookuphandle;

struct lookuphandle	*mp3_lookup_start(int, void *, lookup_fn_t *, const char *);
void			 mp3_lookup_insert(struct lookuphandle *, void *, int);
void			 mp3_lookup_finish(struct lookuphandle *);

void	 mp3_lookup_artist(const char *, struct filler_data *);
void	 mp3_lookup_genre(const char *, struct filler_data *);
void	 mp3_lookup_album(const char *, struct filler_data *);
char	*mp3_gettoken(const char *, int);
int	 mp3_numtoken(const char *);
int      mp3_file_data_for_path(const char *, void *);

enum mp3_filetype mp3_get_filetype(const char *);
#endif
