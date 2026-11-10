; fact.asm - recursive factorial (CALL / RETV), prints 1! .. 10!
        .equ PUTI, 1
main:   ENTER 1
        PUSH 1
        STL 0
loop:   LDL 0
        SYS PUTI
        PUTC '!'
        PUTC ' '
        PUTC '='
        PUTC ' '
        LDL 0
        CALL fact
        SYS PUTI
        PUTC 10
        LDL 0
        INC
        DUP
        STL 0
        PUSH 11
        LT
        JNZ loop
        HALT

fact:                           ; fact(n)
        LDL -1
        PUSH 2
        LT
        JZ rec
        PUSH 1
        RETV 1
rec:    LDL -1
        LDL -1
        DEC
        CALL fact
        MUL
        RETV 1
