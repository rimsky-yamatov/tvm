/* tvm.h - tiny VM: definitions shared by the VM, the host and the assembler */
#ifndef TVM_H
#define TVM_H
#include <stdint.h>

typedef uint32_t u32;
typedef int32_t  i32;

/* operand kinds: none, signed 8, unsigned 8, zigzag-LEB128, relative zigzag-LEB128 */
enum { K_N, K_I8, K_U8, K_V, K_R };

#define TVM_OPS(X) \
  X(HALT,0x00,N)  X(PUSH0,0x01,N) X(PUSH1,0x02,N) X(PUSHI8,0x03,I8) X(PUSHV,0x04,V) \
  X(DUP,0x05,N)   X(DROP,0x06,N)  X(SWAP,0x07,N)  X(OVER,0x08,N)    X(ROT,0x09,N) \
  X(ADD,0x10,N)   X(SUB,0x11,N)   X(MUL,0x12,N)   X(DIV,0x13,N)     X(MOD,0x14,N) \
  X(NEG,0x15,N)   X(AND,0x16,N)   X(OR,0x17,N)    X(XOR,0x18,N)     X(SHL,0x19,N) \
  X(SHR,0x1A,N)   X(INC,0x1B,N)   X(DEC,0x1C,N)   X(NOT,0x1D,N)     X(MIN,0x1E,N) \
  X(MAX,0x1F,N) \
  X(EQ,0x20,N)    X(NE,0x21,N)    X(LT,0x22,N)    X(GT,0x23,N)      X(LE,0x24,N) \
  X(GE,0x25,N) \
  X(JMP,0x30,R)   X(JZ,0x31,R)    X(JNZ,0x32,R)   X(CALL,0x33,R)    X(CALLI,0x34,N) \
  X(RET,0x35,U8)  X(RETV,0x36,U8) X(ENTER,0x37,U8) \
  X(LDL,0x40,I8)  X(STL,0x41,I8)  X(LD8,0x42,N)   X(ST8,0x43,N)     X(LD,0x44,N) \
  X(ST,0x45,N)    X(LD16,0x46,N)  X(ST16,0x47,N)  X(MEMCPY,0x48,N)  X(MEMSET,0x49,N) \
  X(SYS,0x50,U8)  X(PUTC,0x51,U8)

enum {
#define X(n,c,k) OP_##n = c,
  TVM_OPS(X)
#undef X
  OP__END
};

#define STK   1024              /* data stack (power of two; indices are masked) */
#define MEMSZ 65536             /* code + data, 16-bit addresses (wrap around)   */

typedef struct { u32 st[STK]; u32 sp; uint8_t mem[MEMSZ]; } VM;

static inline u32  vpop (VM *v)        { return v->st[--v->sp & (STK-1)]; }
static inline void vpush(VM *v, u32 x) { v->st[v->sp++ & (STK-1)] = x; }

int tvm_run(VM *v);          /* 0 = HALT, 1 = invalid opcode / exit request */
int tvm_sys(VM *v, int n);   /* provided by the host; nonzero = stop the VM  */
#endif
