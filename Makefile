CFLAGS  = -Wall -std=c99 -D_BSD_SOURCE -I/usr/local/include/ -Iinclude/ \
	`pkg-config fuse --cflags` `pkg-config taglib --cflags` -DDEBUGGING

LD_ADD  = -L/usr/local/lib -lsqlite3 -ltag_c \
	`pkg-config fuse --libs`
CC = gcc

C_FILES = $(wildcard src/*.c)
OBJS    = $(C_FILES:.c=.o) \

PROGRAM = musicfs

all: $(PROGRAM)

$(PROGRAM): $(OBJS)
	$(CC) -o $@ $+ $(LD_ADD)

clean:
	rm -f $(PROGRAM) $(OBJS) *~
