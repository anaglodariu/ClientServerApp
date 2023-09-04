CFLAGS = -Wall -g -Werror -Wno-error=unused-variable

all: server subscriber

common.o: common.c

server: server.c common.o
	gcc $(CFLAGS) server.c common.o -o server -lm

subscriber: subscriber.c common.o
	gcc $(CFLAGS) subscriber.c common.o -o subscriber -lm

.PHONY: clean

clean:
	rm -rf server subscriber *.o
