/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Musicfs is a FUSE module implementing a media filesystem in userland.
 * Copyright (C) 2008  Ulf Lilleengen, Kjetil Ørbekk
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * A copy of the license can typically be found in COPYING
 */

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

