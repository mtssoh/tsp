CC     = gcc
CFLAGS = -Wall -Wextra -O2 -fopenmp

SRCS = main.c parser.c ga.c
OBJS = $(SRCS:%.c=build/%.o)

all: build/tsp

build/tsp: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

build/%.o: %.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf build

.PHONY: all clean
