/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
#include <fuse_opt.h>

extern char *sourcedir;

static int mp3fs_opt_proc (void *data, const char *arg, int key,
						   struct fuse_args *outargs)
{
	if (key == FUSE_OPT_KEY_NONOPT && !sourcedir) {
		/* The source directory isn't already set, let's do it */
		sourcedir = arg;
		return (0);
	}
	else
		return (1);
}
