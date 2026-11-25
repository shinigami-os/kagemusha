CC ?= cc
CFLAGS ?= -std=c11 -D_GNU_SOURCE -O2 -g -Wall -Wextra -Werror
EVDI_CFLAGS ?=
EVDI_LIBS ?= -levdi

all: build/kage-edid build/kage build/test-edid build/test-mode build/test-hypr build/test-state build/test-sysfs

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

build/hypr.o: src/hypr.c src/hypr.h | build
	$(CC) $(CFLAGS) -c -o $@ src/hypr.c

build/state.o: src/state.c src/state.h | build
	$(CC) $(CFLAGS) -c -o $@ src/state.c

build/sysfs.o: src/sysfs.c src/sysfs.h | build
	$(CC) $(CFLAGS) -c -o $@ src/sysfs.c

build/kage: src/kage.c build/hypr.o build/state.o build/sysfs.o build/edid.o build/mode.o
	$(CC) $(CFLAGS) -o $@ src/kage.c build/hypr.o build/state.o build/sysfs.o build/edid.o build/mode.o

build/kagemushad: src/kagemushad.c build/vmon.o build/edid.o build/mode.o
	$(CC) $(CFLAGS) $(EVDI_CFLAGS) -o $@ src/kagemushad.c build/vmon.o build/edid.o build/mode.o $(EVDI_LIBS)

build/test-edid: tests/test_edid.c build/edid.o
	$(CC) $(CFLAGS) -o $@ tests/test_edid.c build/edid.o

build/test-mode: tests/test_mode.c build/mode.o
	$(CC) $(CFLAGS) -o $@ tests/test_mode.c build/mode.o

build/test-hypr: tests/test_hypr.c tests/check.h build/hypr.o
	$(CC) $(CFLAGS) -o $@ tests/test_hypr.c build/hypr.o

build/test-state: tests/test_state.c tests/check.h build/state.o
	$(CC) $(CFLAGS) -o $@ tests/test_state.c build/state.o

build/test-sysfs: tests/test_sysfs.c tests/check.h build/sysfs.o
	$(CC) $(CFLAGS) -o $@ tests/test_sysfs.c build/sysfs.o

test: build/test-edid build/test-mode build/test-hypr build/test-state build/test-sysfs
	./build/test-edid
	./build/test-mode
	./build/test-hypr
	./build/test-state
	./build/test-sysfs

clean:
	rm -rf build

.PHONY: all daemon test clean
