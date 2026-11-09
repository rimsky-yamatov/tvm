; primes.asm - sieve of Eratosthenes below 100 (MEMSET / LD8 / ST8)
        .equ PUTI, 1
main:   ENTER 2                 ; 0=i 1=j
        PUSH sieve
        PUSH 1
        PUSH 100
        MEMSET                  ; mark everything as prime
        PUSH 2
        STL 0
outer:  LDL 0
        PUSH sieve
        ADD
        LD8
        JZ next
        LDL 0
        SYS PUTI
        PUTC ' '
        LDL 0
        DUP
        ADD
        STL 1                   ; j = 2i
inner:  LDL 1
        PUSH 100
        LT
        JZ next
        PUSH 0
        LDL 1
        PUSH sieve
        ADD
        ST8                     ; sieve[j] = 0
        LDL 1
        LDL 0
        ADD
        STL 1
        JMP inner
next:   LDL 0
        INC
        DUP
        STL 0
        PUSH 100
        LT
        JNZ outer
        PUTC 10
        HALT
sieve:  .space 100
