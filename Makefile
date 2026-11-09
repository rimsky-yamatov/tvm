CC     ?= cc
CFLAGS ?= -O2 -Wall -Wextra

all: tvm tasm

tvm: main.c vm.c gui_sdl.c tvm.h gui.h
	$(CC) $(CFLAGS) $(shell sdl2-config --cflags) -o $@ main.c vm.c gui_sdl.c $(shell sdl2-config --libs)

tasm: tasm.c tvm.h
	$(CC) $(CFLAGS) -o $@ tasm.c

clean:
	rm -f tvm tasm *.bin
