; fib.asm - first 20 Fibonacci numbers
        .equ PUTI, 1
main:   ENTER 3                 ; 0=a 1=b 2=i
        PUSH 0
        STL 0
        PUSH 1
        STL 1
        PUSH 0
        STL 2
loop:   LDL 0
        SYS PUTI
        PUTC ' '
        LDL 0
        LDL 1
        ADD                     ; a+b
        LDL 1
        STL 0                   ; a = b
        STL 1                   ; b = a+b
        LDL 2
        INC
        DUP
        STL 2
        PUSH 20
        LT
        JNZ loop
        PUTC 10
        HALT
