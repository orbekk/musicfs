/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
#define FUSE_USE_VERSION 26

#include <stdlib.h>
#include <stdio.h>
#include <fuse.h>

#include "mp3_vnops.c"
#include "mp3_opt_parse.c"

/* static struct fuse_module mp3_mod { */
/* 	.name = "MP3FS" */
/* }; */

/* Just for testing the argument parsing. TODO: fix something better */
char *sourcedir = NULL;

int
main(int argc, char **argv)
{
	struct fuse_args args = FUSE_ARGS_INIT (argc, argv);

	if (fuse_opt_parse(&args, NULL, NULL, mp3fs_opt_proc) != 0)
		exit (1);

	return (fuse_main (args.argc, args.argv, &mp3_oper, NULL));
}
