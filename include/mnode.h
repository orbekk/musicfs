#ifndef _MP3NODE_H_
#define _MP3NODE_H_
struct ID3Tag;

struct mnode {
	char *path;
	ID3Tag *tag;

	/* Entry in search structure. */
	LIST_ENTRY(mnode) next;
};
#endif
