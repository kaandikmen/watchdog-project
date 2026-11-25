CC      = gcc
CFLAGS  = -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -g -Iinclude
SRCS    = $(wildcard src/*.c)
OBJS    = $(SRCS:src/%.c=build/%.o)

all: watchdog worker

watchdog: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

worker: workers/worker.c
	$(CC) $(CFLAGS) -o $@ $<

test: all
	bash tests/test_scenarios.sh

clean:
	rm -rf build watchdog worker watchdog.log

.PHONY: all test clean
