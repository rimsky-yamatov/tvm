CC     ?= cc
CFLAGS ?= -O2 -Wall -Wextra

ifeq ($(OS),Windows_NT)
  EXE      := .exe
  SDL_LIBS := -lSDL2                 # console-subsystem exe: no -lSDL2main / -mwindows
else
  EXE      :=
  SDL_LIBS := $(shell sdl2-config --libs)
endif
SDL_CFLAGS := $(filter-out -Dmain=SDL_main,$(shell sdl2-config --cflags))

.PHONY: all clean test

all: tvm$(EXE) tasm$(EXE) tdis$(EXE) tvc$(EXE)

tvm$(EXE): main.c vm.c gui_sdl.c tvm.h gui.h
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $@ main.c vm.c gui_sdl.c $(SDL_LIBS) -lm

tasm$(EXE): tasm.c tvm.h
	$(CC) $(CFLAGS) -o $@ tasm.c

tdis$(EXE): tdis.c tvm.h
	$(CC) $(CFLAGS) -o $@ tdis.c

tvc$(EXE): tvc.c
	$(CC) $(CFLAGS) -o $@ tvc.c

test:
	sh tests/run.sh

clean:
	rm -f tvm tasm tdis tvc *.exe *.tvb
