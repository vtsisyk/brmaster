CC = gcc
CFLAGS = -Wall -Werror -Wextra $(shell pkg-config --cflags glib-2.0)
LDFLAGS = -lmnl $(shell pkg-config --libs glib-2.0)

all: brmaster
brmaster: brmaster.c
	$(CC) $(CFLAGS)  brmaster.c -o brmaster $(LDFLAGS)
clean:
	rm brmaster
