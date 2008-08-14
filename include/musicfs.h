#ifndef _MP3FS_H_
#define _MP3FS_H_
struct fuse_args;

#define DBNAME "music.db"

int	mfs_run(int, char **);
int	mfs_initscan(char *);


/* 
 * Functions traversing the underlying filesystem and do operations on the
 * files, for instance scanning the collection.
 */
typedef void traverse_fn_t(char *);
void traverse_hierarchy(char *, traverse_fn_t);
traverse_fn_t mfs_scan;

/*
 * Data passed to mfs_lookup_list
 */
struct filler_data {
	void *buf;
	fuse_fill_dir_t filler;
};

/*
 * Data passed to mfs_lookup_open
 */
struct file_data {
	int fd;
	int found;
};

#define LIST_DATATYPE_STRING 1
#define LIST_DATATYPE_INT 2

enum mfs_filetype { MFS_NOTFOUND, MFS_FILE, MFS_DIRECTORY };

/* A lookup functions returns 0 if it's finished. */
typedef int lookup_fn_t(void *, const char *);
/* Lookup function listing each row. */
lookup_fn_t mfs_lookup_list;
/* Lookup function opening the first file match. */
lookup_fn_t mfs_lookup_open;
/* Lookup function stat'ing a file. */
lookup_fn_t mfs_lookup_stat;

struct lookuphandle;

struct lookuphandle	*mfs_lookup_start(int, void *, lookup_fn_t *, const char *);
void			 mfs_lookup_insert(struct lookuphandle *, void *, int);
void			 mfs_lookup_finish(struct lookuphandle *);

void	 mfs_lookup_artist(const char *, struct filler_data *);
void	 mfs_lookup_genre(const char *, struct filler_data *);
void	 mfs_lookup_album(const char *, struct filler_data *);
char	*mfs_gettoken(const char *, int);
int	 mfs_numtoken(const char *);
int      mfs_file_data_for_path(const char *, void *);

enum mfs_filetype mfs_get_filetype(const char *);
#endif
