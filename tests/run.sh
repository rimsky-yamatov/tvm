#!/bin/sh
# Smoke tests: assemble every example, compare console output, replay scripted GUI events headlessly.
set -e
cd "$(dirname "$0")/.."
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
make -s
fail=0
CC=${CC:-cc}
SDLC=$(sdl2-config --cflags | sed 's/-Dmain=SDL_main//')
case "$(uname -s)" in MINGW*|MSYS*) SDLL="-lSDL2" ;; *) SDLL=$(sdl2-config --libs) ;; esac
TO=$(command -v timeout || command -v gtimeout || true)
hash() { if command -v md5sum >/dev/null 2>&1; then md5sum; else md5 -r; fi | cut -d' ' -f1; }

for f in examples/*.tas; do
    n=$(basename "$f" .tas)
    ./tasm "$f" "$T/$n.tvb" 2>/dev/null || { echo "FAIL assemble $n"; fail=1; }
done

for n in hello fib fact primes fixed; do
    ./tvm "$T/$n.tvb" > "$T/$n.out"
    if cmp -s "$T/$n.out" "tests/expected/$n.txt"; then echo "ok   $n"; else echo "FAIL $n"; fail=1; fi
done

# stdin, EXIT(code)
rc=0; printf 'hello\nworld\n' | ./tvm "$T/echo.tvb" > "$T/echo.out" || rc=$?
if [ $rc -eq 2 ] && cmp -s "$T/echo.out" tests/expected/echo.txt; then echo "ok   echo (stdin, exit code)"; else echo "FAIL echo (exit $rc)"; fail=1; fi

# disassembler
./tdis "$T/hello.tvb" 0 8 > "$T/hello.dis"
if cmp -s "$T/hello.dis" tests/expected/hello.dis; then echo "ok   tdis"; else echo "FAIL tdis"; fail=1; fi

# portable switch dispatch (no computed goto)
$CC -O2 -DTVM_NO_GOTO $SDLC -o "$T/tvm_sw" main.c vm.c gui_sdl.c $SDLL -lm
if "$T/tvm_sw" "$T/fib.tvb" | cmp -s - tests/expected/fib.txt; then echo "ok   fib (switch dispatch)"; else echo "FAIL switch dispatch"; fail=1; fi

# GUI: replay scripted events with the dummy video driver
F="$SDLC -O2"
$CC $F -c -Dgui_poll=real_poll gui_sdl.c -o "$T/g.o"
$CC $F -c main.c -o "$T/m.o"
$CC $F -c vm.c -o "$T/v.o"
$CC $F -c tests/gui_events.c -o "$T/e.o"
$CC -o "$T/tvm_events" "$T/m.o" "$T/v.o" "$T/g.o" "$T/e.o" $SDLL -lm
SDL_VIDEODRIVER=dummy TVM_DUMP="$T/w" "$T/tvm_events" "$T/win2.tvb"
sum=$(cat "$T/w0.ppm" "$T/w1.ppm" | hash)
if [ "$sum" = "$(cat tests/expected/win2.md5)" ]; then echo "ok   win2 (scripted events)"; else echo "FAIL win2"; fail=1; fi

# every GUI example must start and keep running (timeout = 124) with the dummy video driver
if [ -n "$TO" ]; then
    for n in lines bounce paint counter circles sprite stopwatch; do
        rc=0; SDL_VIDEODRIVER=dummy $TO 1 ./tvm "$T/$n.tvb" > /dev/null 2>&1 || rc=$?
        if [ $rc -eq 124 ]; then echo "ok   $n (runs)"; else echo "FAIL $n (exit $rc)"; fail=1; fi
    done
else
    echo "skip GUI smoke runs (no timeout command)"
fi
# audio example with the dummy audio driver
rc=0; SDL_AUDIODRIVER=dummy ./tvm "$T/music.tvb" || rc=$?
if [ $rc -eq 0 ]; then echo "ok   music"; else echo "FAIL music (exit $rc)"; fail=1; fi

exit $fail
