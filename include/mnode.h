#ifndef _MP3NODE_H_
#define _MP3NODE_H_
struct TagLib_File;

struct mnode {
	char *path;
	TagLib_File *tag;
	/* Entry in search structure. */
	LIST_ENTRY(mnode) next;
};
#endif
