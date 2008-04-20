/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
#define FUSE_USE_VERSION 26

#include <stdlib.h>
#include <stdio.h>
#include <fuse.h>
#include <fuse_opt.h>
#include <mp3fs.h>

/* Prototypes. */
static int mp3fs_opt_proc (void *, const char *, int, struct fuse_args *);

/* Just for testing the argument parsing. TODO: fix something better */
char *sourcedir = NULL;

int
main(int argc, char **argv)
{
	struct fuse_args args = FUSE_ARGS_INIT (argc, argv);

	if (fuse_opt_parse(&args, NULL, NULL, mp3fs_opt_proc) != 0)
		exit (1);

	printf("Starting up mp3fs\n");
	mp3_run(&args);
	printf("Shutting down mp3fs\n");
}

static int mp3fs_opt_proc (void *data, const char *arg, int key,
						   struct fuse_args *outargs)
{
	if (key == FUSE_OPT_KEY_NONOPT && !sourcedir) {
		/* The source directory isn't already set, let's do it */
		sourcedir = arg;
		return (0);
	}
	return (-1);
}
