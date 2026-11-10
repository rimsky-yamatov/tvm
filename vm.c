/* vm.c - tiny VM core. No libc, no OS, no allocation. */
#include "tvm.h"

#if defined(__GNUC__) && !defined(TVM_NO_GOTO)
# define USE_GOTO 1
#endif

#define M       (STK-1)
#define PUSH(x) (st[sp++ & M] = (u32)(x))
#define POP()   (st[--sp & M])
#define TOP     (st[(sp-1) & M])
#define A16(x)  ((uint16_t)(x))
#define BIN(e)  do { b = POP(); a = TOP; TOP = (u32)(e); } while (0)
#define RDV(o)  do { u32 z_ = 0; int s_ = 0; unsigned b_; \
    do { b_ = mem[pc++]; z_ |= (u32)(b_ & 127) << s_; s_ += 7; } while ((b_ & 128) && s_ < 35); \
    o = (i32)(z_ >> 1) ^ -(i32)(z_ & 1); } while (0)

int tvm_run(VM *v)
{
    uint8_t *mem = v->mem;
    u32 *st = v->st;
    u32 sp = v->sp, fp = 0, a, b, c, rs[256];
    uint16_t pc = v->entry;
    uint8_t rp = 0;
    i32 r;

#ifdef USE_GOTO
    static void *T[256];
    static int ready;
    if (!ready) {
        int i;
        for (i = 0; i < 256; i++) T[i] = &&L_BAD;
#define X(n,c,k) T[c] = &&L_##n;
        TVM_OPS(X)
#undef X
        ready = 1;
    }
# define CASE(n) L_##n:
# define NEXT()  goto *T[mem[pc++]]
    NEXT();
#else
# define CASE(n) case OP_##n:
# define NEXT()  break
    for (;;) switch (mem[pc++]) {
#endif

    CASE(HALT)   goto done;
    CASE(PUSH0)  PUSH(0); NEXT();
    CASE(PUSH1)  PUSH(1); NEXT();
    CASE(PUSHI8) PUSH((i32)(int8_t)mem[pc++]); NEXT();
    CASE(PUSHV)  RDV(r); PUSH(r); NEXT();
    CASE(DUP)    a = TOP; PUSH(a); NEXT();
    CASE(DROP)   sp--; NEXT();
    CASE(SWAP)   b = POP(); a = TOP; TOP = b; PUSH(a); NEXT();
    CASE(OVER)   a = st[(sp-2) & M]; PUSH(a); NEXT();
    CASE(ROT)    c = POP(); b = POP(); a = POP(); PUSH(b); PUSH(c); PUSH(a); NEXT();

    CASE(ADD)    BIN(a + b); NEXT();
    CASE(SUB)    BIN(a - b); NEXT();
    CASE(MUL)    BIN(a * b); NEXT();
    CASE(DIV)    BIN(b ? (b == 0xFFFFFFFFu ? 0u - a : (u32)((i32)a / (i32)b)) : 0); NEXT();
    CASE(MOD)    BIN(b && b != 0xFFFFFFFFu ? (u32)((i32)a % (i32)b) : 0); NEXT();
    CASE(NEG)    TOP = 0u - TOP; NEXT();
    CASE(AND)    BIN(a & b); NEXT();
    CASE(OR)     BIN(a | b); NEXT();
    CASE(XOR)    BIN(a ^ b); NEXT();
    CASE(SHL)    BIN(a << (b & 31)); NEXT();
    CASE(SHR)    BIN((u32)((i32)a >> (b & 31))); NEXT();
    CASE(INC)    TOP = TOP + 1; NEXT();
    CASE(DEC)    TOP = TOP - 1; NEXT();
    CASE(NOT)    TOP = ~TOP; NEXT();
    CASE(MIN)    BIN((i32)a < (i32)b ? a : b); NEXT();
    CASE(MAX)    BIN((i32)a > (i32)b ? a : b); NEXT();

    CASE(EQ)     BIN(a == b); NEXT();
    CASE(NE)     BIN(a != b); NEXT();
    CASE(LT)     BIN((i32)a <  (i32)b); NEXT();
    CASE(GT)     BIN((i32)a >  (i32)b); NEXT();
    CASE(LE)     BIN((i32)a <= (i32)b); NEXT();
    CASE(GE)     BIN((i32)a >= (i32)b); NEXT();
    CASE(FMUL)   BIN((u32)(((int64_t)(i32)a * (i32)b) >> 16)); NEXT();          /* 16.16 fixed point */
    CASE(FDIV)   BIN(b ? (u32)(((int64_t)(i32)a * 65536) / (i32)b) : 0); NEXT();
    CASE(ABS)    TOP = (i32)TOP < 0 ? 0u - TOP : TOP; NEXT();

    CASE(JMP)    RDV(r); pc = A16(pc + r); NEXT();
    CASE(JZ)     RDV(r); if (!POP()) pc = A16(pc + r); NEXT();
    CASE(JNZ)    RDV(r); if (POP())  pc = A16(pc + r); NEXT();
    CASE(CALL)   RDV(r); rs[rp++] = pc | (fp << 16); fp = sp & M; pc = A16(pc + r); NEXT();
    CASE(CALLI)  a = POP(); rs[rp++] = pc | (fp << 16); fp = sp & M; pc = A16(a); NEXT();
    CASE(RET)    b = mem[pc++]; a = rs[--rp]; sp = fp - b; pc = A16(a); fp = a >> 16; NEXT();
    CASE(RETV)   b = mem[pc++]; c = POP(); a = rs[--rp]; sp = fp - b; PUSH(c); pc = A16(a); fp = a >> 16; NEXT();
    CASE(ENTER)  b = mem[pc++]; while (b--) PUSH(0); NEXT();

    CASE(LDL)    r = (int8_t)mem[pc++]; PUSH(st[(fp + r) & M]); NEXT();
    CASE(STL)    r = (int8_t)mem[pc++]; st[(fp + r) & M] = POP(); NEXT();
    CASE(LD8)    TOP = mem[A16(TOP)]; NEXT();
    CASE(ST8)    a = POP(); b = POP(); mem[A16(a)] = (uint8_t)b; NEXT();
    CASE(LD16)   a = TOP; TOP = (u32)mem[A16(a)] | (u32)mem[A16(a+1)] << 8; NEXT();
    CASE(ST16)   a = POP(); b = POP(); mem[A16(a)] = (uint8_t)b; mem[A16(a+1)] = (uint8_t)(b >> 8); NEXT();
    CASE(LD)     a = TOP;
                 TOP = (u32)mem[A16(a)] | (u32)mem[A16(a+1)] << 8 | (u32)mem[A16(a+2)] << 16 | (u32)mem[A16(a+3)] << 24;
                 NEXT();
    CASE(ST)     a = POP(); b = POP();
                 mem[A16(a)] = (uint8_t)b; mem[A16(a+1)] = (uint8_t)(b >> 8);
                 mem[A16(a+2)] = (uint8_t)(b >> 16); mem[A16(a+3)] = (uint8_t)(b >> 24);
                 NEXT();
    CASE(MEMCPY) c = POP() & 0xFFFF; b = POP(); a = POP(); while (c--) mem[A16(a++)] = mem[A16(b++)]; NEXT();
    CASE(MEMSET) c = POP() & 0xFFFF; b = POP(); a = POP(); while (c--) mem[A16(a++)] = (uint8_t)b; NEXT();

    CASE(SYS)    a = mem[pc++]; v->sp = sp; if (tvm_sys(v, (int)a)) return 1; sp = v->sp; NEXT();
    CASE(PUTC)   PUSH(mem[pc++]); v->sp = sp; tvm_sys(v, 0); sp = v->sp; NEXT();

#ifdef USE_GOTO
    L_BAD:
#else
    default:
#endif
    v->sp = sp;
    return 1;
#ifndef USE_GOTO
    }
#endif
done:
    v->sp = sp;
    return 0;
}
