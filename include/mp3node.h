#ifndef _MP3NODE_H_
#define _MP3NODE_H_
struct ID3Tag;

struct mnode {
	ID3Tag *tag;
	void *data;
};
#endif
