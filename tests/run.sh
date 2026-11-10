#!/bin/sh
# Smoke tests: assemble every example, compare console output, replay scripted GUI events headlessly.
set -e
cd "$(dirname "$0")/.."
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
make -s
fail=0

for f in examples/*.asm; do
    n=$(basename "$f" .asm)
    ./tasm "$f" "$T/$n.bin" 2>/dev/null || { echo "FAIL assemble $n"; fail=1; }
done

for n in hello fib fact primes fixed; do
    ./tvm "$T/$n.bin" > "$T/$n.out"
    if cmp -s "$T/$n.out" "tests/expected/$n.txt"; then echo "ok   $n"; else echo "FAIL $n"; fail=1; fi
done

# stdin, EXIT(code)
rc=0; printf 'hello\nworld\n' | ./tvm "$T/echo.bin" > "$T/echo.out" || rc=$?
if [ $rc -eq 2 ] && cmp -s "$T/echo.out" tests/expected/echo.txt; then echo "ok   echo (stdin, exit code)"; else echo "FAIL echo (exit $rc)"; fail=1; fi

# disassembler
./tdis "$T/hello.bin" 0 8 > "$T/hello.dis"
if cmp -s "$T/hello.dis" tests/expected/hello.dis; then echo "ok   tdis"; else echo "FAIL tdis"; fail=1; fi

# portable switch dispatch (no computed goto)
cc -O2 -DTVM_NO_GOTO $(sdl2-config --cflags) -o "$T/tvm_sw" main.c vm.c gui_sdl.c $(sdl2-config --libs) -lm
if "$T/tvm_sw" "$T/fib.bin" | cmp -s - tests/expected/fib.txt; then echo "ok   fib (switch dispatch)"; else echo "FAIL switch dispatch"; fail=1; fi

# GUI: replay scripted events with the dummy video driver
F="$(sdl2-config --cflags) -O2"
cc $F -c -Dgui_poll=real_poll gui_sdl.c -o "$T/g.o"
cc $F -c main.c -o "$T/m.o"
cc $F -c vm.c -o "$T/v.o"
cc $F -c tests/gui_events.c -o "$T/e.o"
cc -o "$T/tvm_events" "$T/m.o" "$T/v.o" "$T/g.o" "$T/e.o" $(sdl2-config --libs) -lm
SDL_VIDEODRIVER=dummy TVM_DUMP="$T/w" "$T/tvm_events" "$T/win2.bin"
sum=$(cat "$T/w0.ppm" "$T/w1.ppm" | md5sum | cut -d' ' -f1)
if [ "$sum" = "$(cat tests/expected/win2.md5)" ]; then echo "ok   win2 (scripted events)"; else echo "FAIL win2"; fail=1; fi

# every GUI example must start and keep running (timeout = 124) with the dummy video driver
for n in lines bounce paint counter circles sprite stopwatch; do
    rc=0; SDL_VIDEODRIVER=dummy timeout 1 ./tvm "$T/$n.bin" > /dev/null 2>&1 || rc=$?
    if [ $rc -eq 124 ]; then echo "ok   $n (runs)"; else echo "FAIL $n (exit $rc)"; fail=1; fi
done
# audio example with the dummy audio driver
rc=0; SDL_AUDIODRIVER=dummy ./tvm "$T/music.bin" || rc=$?
if [ $rc -eq 0 ]; then echo "ok   music"; else echo "FAIL music (exit $rc)"; fail=1; fi

exit $fail
