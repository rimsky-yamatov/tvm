; echo.asm - stdin to stdout in upper case (GETC / PUTC). Exit code = number of lines (EXIT).
        .equ GETC, 2
        .equ EXIT, 99
main:   ENTER 1                 ; 0 = line count
loop:   SYS GETC
        DUP
        PUSH -1
        EQ
        JNZ eof
        DUP
        PUSH 97                 ; 'a'
        LT
        JNZ out
        DUP
        PUSH 122                ; 'z'
        GT
        JNZ out
        PUSH 32
        SUB                     ; to upper case
out:    DUP
        PUSH 10
        EQ
        JZ pr
        LDL 0
        INC
        STL 0
pr:     SYS 0                   ; PUTC
        JMP loop
eof:    DROP
        LDL 0
        SYS EXIT
