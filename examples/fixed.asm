; fixed.asm - 16.16 fixed point and math helpers: SQRT, FMUL, FDIV, SIN, COS, ABS
        .equ PUTI, 1
        .equ SIN,  40
        .equ COS,  41
        .equ SQRT, 42
        .equ ONE,  65536
main:   PUSH 144
        SYS SQRT
        SYS PUTI                ; 12
        PUTC ' '
        PUSH ONE*3/2            ; 1.5
        PUSH ONE*5/2            ; 2.5
        FMUL
        SYS PUTI                ; 3.75 = 245760
        PUTC ' '
        PUSH ONE
        PUSH ONE*3
        FDIV
        SYS PUTI                ; 1/3 = 21845
        PUTC ' '
        PUSH 30
        SYS SIN
        SYS PUTI                ; 0.5 = 32768
        PUTC ' '
        PUSH 60
        SYS COS
        SYS PUTI                ; 0.5 = 32768
        PUTC ' '
        PUSH -7
        ABS
        SYS PUTI                ; 7
        PUTC 10
        HALT
