CC     ?= cc
CFLAGS ?= -O2 -Wall -Wextra

.PHONY: all clean test

all: tvm tasm tdis

tvm: main.c vm.c gui_sdl.c tvm.h gui.h
	$(CC) $(CFLAGS) $(shell sdl2-config --cflags) -o $@ main.c vm.c gui_sdl.c $(shell sdl2-config --libs) -lm

tasm: tasm.c tvm.h
	$(CC) $(CFLAGS) -o $@ tasm.c

tdis: tdis.c tvm.h
	$(CC) $(CFLAGS) -o $@ tdis.c

test:
	sh tests/run.sh

clean:
	rm -f tvm tasm tdis *.tvm
