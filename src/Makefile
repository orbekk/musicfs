CFLAGS = -Wall -std=c99 -D_BSD_SOURCE -g \
    `pkg-config fuse --cflags` `pkg-config taglib --cflags` \
    -DDEBUGGING -DSQLITE_THREADED

INCLUDES= -I/usr/local/include -I../include -I../pnotify
LDFLAGS= -L/usr/local/lib -L../pnotify
LIBS= -lsqlite3 -ltag_c -lpthread -lpnotify `pkg-config fuse --libs`
CC= gcc
LD= gcc
SRCS= mfs_cleanup_db.c mfs_subr.c mfs_vnops.c musicfs.c
OBJS= $(SRCS:.c=.o)

PROGRAM = musicfs

all: $(PROGRAM)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(PROGRAM): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)

clean:
	rm -f $(PROGRAM) $(OBJS) *~
