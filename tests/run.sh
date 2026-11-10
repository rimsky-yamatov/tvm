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
# compare two files ignoring CR (a Windows checkout may turn LF into CRLF); print the first differences on failure
same() {
    tr -d '\r' < "$1" > "$T/.cmp_a"; tr -d '\r' < "$2" > "$T/.cmp_b"
    if cmp -s "$T/.cmp_a" "$T/.cmp_b"; then return 0; fi
    echo "--- differences: $1 vs $2" >&2; diff "$T/.cmp_a" "$T/.cmp_b" 2>&1 | head -6 >&2
    return 1
}

for f in examples/*.tas; do
    n=$(basename "$f" .tas)
    ./tasm "$f" "$T/$n.tvb" 2>/dev/null || { echo "FAIL assemble $n"; fail=1; }
done

for n in hello fib fact primes fixed; do
    ./tvm "$T/$n.tvb" > "$T/$n.out"
    if same "$T/$n.out" "tests/expected/$n.txt"; then echo "ok   $n"; else echo "FAIL $n"; fail=1; fi
done

# stdin, EXIT(code)
rc=0; printf 'hello\nworld\n' | ./tvm "$T/echo.tvb" > "$T/echo.out" || rc=$?
if [ $rc -eq 2 ] && same "$T/echo.out" tests/expected/echo.txt; then echo "ok   echo (stdin, exit code)"; else echo "FAIL echo (exit $rc)"; fail=1; fi

# sandboxed file I/O: works with -fs DIR, ".." is refused, everything fails without -fs
./tvm -fs "$T" "$T/files.tvb" > "$T/files.out"
if same "$T/files.out" tests/expected/files.txt && [ ! -e "$T/out.txt" ]; then echo "ok   files (-fs sandbox)"; else echo "FAIL files"; fail=1; fi
rc=0; ./tvm "$T/files.tvb" > "$T/nofs.out" || rc=$?
if [ $rc -eq 1 ] && same "$T/nofs.out" tests/expected/nofs.txt; then echo "ok   files (disabled without -fs)"; else echo "FAIL files without -fs"; fail=1; fi

# stdout must be written byte for byte (no CRLF translation on Windows): "Hello, World!\n" is 14 bytes
n=$(./tvm "$T/hello.tvb" | wc -c | tr -d ' ')
if [ "$n" = "14" ]; then echo "ok   output is plain LF"; else echo "FAIL output is $n bytes, expected 14"; fail=1; fi

# disassembler
./tdis "$T/hello.tvb" 0 8 > "$T/hello.dis"
if same "$T/hello.dis" tests/expected/hello.dis; then echo "ok   tdis"; else echo "FAIL tdis"; fail=1; fi

# portable switch dispatch (no computed goto)
$CC -O2 -DTVM_NO_GOTO $SDLC -o "$T/tvm_sw" main.c vm.c gui_sdl.c $SDLL -lm
"$T/tvm_sw" "$T/fib.tvb" > "$T/fib_sw.out"
if same "$T/fib_sw.out" tests/expected/fib.txt; then echo "ok   fib (switch dispatch)"; else echo "FAIL switch dispatch"; fail=1; fi

# GUI: replay scripted events with the dummy video driver
F="$SDLC -O2"
$CC $F -c -Dgui_poll=real_poll gui_sdl.c -o "$T/g.o"
$CC $F -c main.c -o "$T/m.o"
$CC $F -c vm.c -o "$T/v.o"
$CC $F -c tests/gui_events.c -o "$T/e.o"
$CC -o "$T/tvm_events" "$T/m.o" "$T/v.o" "$T/g.o" "$T/e.o" $SDLL -lm
SDL_VIDEODRIVER=dummy TVM_DUMP="$T/w" "$T/tvm_events" "$T/win2.tvb"
sum=$(cat "$T/w0.ppm" "$T/w1.ppm" | hash)
if [ "$sum" = "$(tr -d '\r' < tests/expected/win2.md5)" ]; then echo "ok   win2 (scripted events)"; else echo "FAIL win2"; fail=1; fi

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

# ---- C-like language: tvc -> tasm -> tvm ----
for f in examples/tc/*.tc tests/lang.tc; do
    n=$(basename "$f" .tc)
    if ./tvc "$f" "$T/tc_$n.tas" && ./tasm "$T/tc_$n.tas" "$T/tc_$n.tvb" 2>/dev/null; then :; else echo "FAIL compile $n"; fail=1; fi
done
for n in hello fizzbuzz sort; do
    ./tvm "$T/tc_$n.tvb" > "$T/tc_$n.out"
    if same "$T/tc_$n.out" "tests/expected/tc_$n.txt"; then echo "ok   tc $n"; else echo "FAIL tc $n"; fail=1; fi
done
./tvm "$T/tc_primes.tvb" > "$T/tc_primes.out"
if same "$T/tc_primes.out" tests/expected/primes.txt; then echo "ok   tc primes (same output as the assembly version)"; else echo "FAIL tc primes"; fail=1; fi
./tvm -fs "$T" "$T/tc_files.tvb" > "$T/tc_files.out"
if same "$T/tc_files.out" tests/expected/tc_files.txt && [ ! -e "$T/numbers.txt" ]; then echo "ok   tc files"; else echo "FAIL tc files"; fail=1; fi
rc=0; ./tvm "$T/tc_lang.tvb" > "$T/tc_lang.out" || rc=$?
if [ $rc -eq 3 ] && same "$T/tc_lang.out" tests/expected/lang.txt; then echo "ok   tc lang (all language features, exit code 3)"; else echo "FAIL tc lang (exit $rc)"; fail=1; fi

check_err() {   # source text, expected message fragment
    printf '%s\n' "$1" > "$T/err.tc"
    if ./tvc "$T/err.tc" "$T/err.tas" 2> "$T/err.out"; then echo "FAIL (accepted) $2"; fail=1
    elif grep -q "$2" "$T/err.out"; then echo "ok   error: $2"; else echo "FAIL (message) $2"; fail=1; fi
}
check_err 'void main() { x = 1; }' "undeclared identifier"
check_err 'void main() { puti(1, 2); }' "expects 1 argument"
check_err 'void main() { int a[4]; }' "local arrays"
check_err 'int f() { return 1; }' "no main function"
check_err 'void main() { int x = f(); } void f() { }' "void function used as a value"
check_err 'void main() { break; }' "outside of a loop"

if [ -n "$TO" ]; then
    for n in bounce mandelbrot life paint; do
        rc=0; SDL_VIDEODRIVER=dummy $TO 1 ./tvm "$T/tc_$n.tvb" > /dev/null 2>&1 || rc=$?
        if [ $rc -eq 124 ]; then echo "ok   tc $n (runs)"; else echo "FAIL tc $n (exit $rc)"; fail=1; fi
    done
fi

exit $fail
