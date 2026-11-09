; bounce.asm - a bouncing square (animation loop with FLIP + SLEEP). Esc / close quits.
        .equ WIN,   10
        .equ RECT,  21
        .equ CLEAR, 24
        .equ FLIP,  26
        .equ POLL,  30
        .equ SLEEP, 32
main:   ENTER 8                 ; 0=x 1=y 2=dx 3=dy  4..7=event
        PUSH 200
        PUSH 150
        SYS WIN
        DROP
        PUSH 20
        STL 0
        PUSH 30
        STL 1
        PUSH 3
        STL 2
        PUSH 2
        STL 3

frame:  SYS POLL
        STL 7
        STL 6
        STL 5
        STL 4
        LDL 5
        PUSH 1
        EQ
        JNZ quit
        LDL 5
        PUSH 6
        EQ
        JNZ quit
        LDL 5
        PUSH 2
        EQ
        JZ move
        LDL 6
        PUSH 27
        EQ
        JNZ quit
move:   LDL 0                   ; x += dx, bounce at 0 / 192
        LDL 2
        ADD
        STL 0
        LDL 0
        PUSH 0
        LT
        JNZ flipx
        LDL 0
        PUSH 192
        GT
        JZ movey
flipx:  LDL 2
        NEG
        STL 2
movey:  LDL 1                   ; y += dy, bounce at 0 / 142
        LDL 3
        ADD
        STL 1
        LDL 1
        PUSH 0
        LT
        JNZ flipy
        LDL 1
        PUSH 142
        GT
        JZ draw
flipy:  LDL 3
        NEG
        STL 3
draw:   PUSH 0
        SYS CLEAR
        LDL 0
        LDL 1
        PUSH 8
        PUSH 8
        PUSH 14
        SYS RECT
        SYS FLIP
        PUSH 16
        SYS SLEEP
        JMP frame
quit:   HALT
