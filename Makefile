CC ?= cc
CFLAGS ?= -std=c11 -O2 -g -Wall -Wextra -Werror

all: build/kage-edid build/test-edid

build:
	mkdir -p build

build/edid.o: src/edid.c src/edid.h | build
	$(CC) $(CFLAGS) -c -o $@ src/edid.c

build/kage-edid: tools/kage-edid.c build/edid.o
	$(CC) $(CFLAGS) -o $@ tools/kage-edid.c build/edid.o

build/test-edid: tests/test_edid.c build/edid.o
	$(CC) $(CFLAGS) -o $@ tests/test_edid.c build/edid.o

test: build/test-edid
	./build/test-edid

clean:
	rm -rf build

.PHONY: all test clean
