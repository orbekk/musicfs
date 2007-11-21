CFLAGS  = -Wall -Iinclude/ \
	`pkg-config fuse --cflags` 

LD_ADD  = -lid3 \
	`pkg-config fuse --libs`
CC = gcc

C_FILES = $(wildcard src/*.c)
OBJS    = $(C_FILES:.c=.o) \

PROGRAM = mp3fs

all: $(PROGRAM)

$(PROGRAM): $(OBJS)
	$(CC) -o $@ $+ $(LD_ADD)

clean:
	rm -f $(PROGRAM) $(OBJS) *~
