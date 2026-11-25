CC ?= cc
CFLAGS ?= -std=c11 -D_GNU_SOURCE -O2 -g -Wall -Wextra -Werror
EVDI_CFLAGS ?=
EVDI_LIBS ?= -levdi

all: build/kage-edid build/test-edid build/test-mode

daemon: build/kagemushad

build:
	mkdir -p build

build/edid.o: src/edid.c src/edid.h | build
	$(CC) $(CFLAGS) -c -o $@ src/edid.c

build/mode.o: src/mode.c src/mode.h | build
	$(CC) $(CFLAGS) -c -o $@ src/mode.c

build/vmon.o: src/vmon.c src/vmon.h src/edid.h | build
	$(CC) $(CFLAGS) $(EVDI_CFLAGS) -c -o $@ src/vmon.c

build/kage-edid: tools/kage-edid.c build/edid.o build/mode.o
	$(CC) $(CFLAGS) -o $@ tools/kage-edid.c build/edid.o build/mode.o

build/kagemushad: src/kagemushad.c build/vmon.o build/edid.o build/mode.o
	$(CC) $(CFLAGS) $(EVDI_CFLAGS) -o $@ src/kagemushad.c build/vmon.o build/edid.o build/mode.o $(EVDI_LIBS)

build/test-edid: tests/test_edid.c build/edid.o
	$(CC) $(CFLAGS) -o $@ tests/test_edid.c build/edid.o

build/test-mode: tests/test_mode.c build/mode.o
	$(CC) $(CFLAGS) -o $@ tests/test_mode.c build/mode.o

test: build/test-edid build/test-mode
	./build/test-edid
	./build/test-mode

clean:
	rm -rf build

.PHONY: all daemon test clean
