/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <fuse.h>
#include "mp3_vnops.c"

/* static struct fuse_module mp3_mod { */
/* 	.name = "MP3FS" */
/* }; */

int
main(int argc, char **argv)
{
	return fuse_main (argc, argv, &mp3_oper);
}
