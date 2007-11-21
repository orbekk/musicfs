/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

extern int errno;

void print_directory (char *path, int level)
{
	DIR *dir = opendir (path);
	int i;

	if (dir == NULL) {
		for (i=0; i<level; i++) printf (" ");
		if (errno == EACCES)
			printf ("* No access\n");
		return;
	}

	struct dirent *de;

	while ((de = readdir (dir)) != NULL) {
		if (strcmp (de->d_name, "." ) == 0 ||
			strcmp (de->d_name, "..") == 0)
			continue;

		for (i=0; i<level; i++) printf (" ");
		printf ("- %s\n", de->d_name);

		if (de->d_type == 4) { /* ToDo: have to use lstat or something */
			char subdir[strlen(path) + strlen(de->d_name) + 1];
			strcpy (subdir, path);
			strcpy (subdir+strlen(path), "/");
			strcpy (subdir+strlen(path)+1, de->d_name);
			
			print_directory (subdir, level+1);
		}
	}
}

int main (int argc, char **argv)
{
	if (argc != 2)
		return -1;

	print_directory (argv[1], 0);
	return 0;
}

