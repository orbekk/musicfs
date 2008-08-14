/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
#define FUSE_USE_VERSION 26

#include <stdlib.h>
#include <stdio.h>
#include <fuse.h>
#include <fuse_opt.h>
#include <musicfs.h>

/* Prototypes. */
static int musicfs_opt_proc (void *, const char *, int, struct fuse_args *);

int
main(int argc, char **argv)
{
	printf("Starting up musicfs\n");
	mfs_run(argc, argv);
	printf("Shutting down musicfs\n");
}

