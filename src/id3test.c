/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
#include <id3.h>
#include <stdio.h>
#include "mp3node.h"

/* Compile with gcc -Wall -lid3 -I../include id3test.c */

int main (int argc, char **argv)
{
	if (argc != 2)
		return -1;

	struct mnode node;
	
	node.tag = ID3Tag_New ();
	ID3Tag_Link (node.tag, argv[1]);
	ID3Frame *artist_frame = ID3Tag_FindFrameWithID (node.tag, ID3FID_LEADARTIST);

	ID3Field *field = ID3Frame_GetField (artist_frame, ID3FN_TEXT);
	char *artist = malloc (sizeof(char) * (ID3Field_Size (field) + 1));
	ID3Field_GetASCII (field, artist, 254);

	printf("Artist: %s\n", artist);

	return 0;
}
