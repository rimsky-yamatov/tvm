; circles.asm - 12 dots orbit a ring: CIRCLE / FCIRCLE with SIN / COS. Esc / close quits.
        .equ WIN,     10
        .equ CIRCLE,  16
        .equ FCIRCLE, 17
        .equ CLEAR,   24
        .equ FLIP,    26
        .equ POLL,    30
        .equ SLEEP,   32
        .equ SIN,     40
        .equ COS,     41
main:   ENTER 8                 ; 0=t 1=k 2=angle  4..7=event
        PUSH 240
        PUSH 240
        SYS WIN
        DROP
        PUSH 0
        STL 0
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
        JZ draw
        LDL 6
        PUSH 27
        EQ
        JNZ quit
draw:   PUSH 0
        SYS CLEAR
        PUSH 120
        PUSH 120
        PUSH 90
        PUSH 8
        SYS CIRCLE
        PUSH 0
        STL 1
dot:    LDL 1
        PUSH 30
        MUL
        LDL 0
        ADD
        STL 2                   ; angle = k*30 + t
        PUSH 120
        LDL 2
        SYS COS
        PUSH 90
        MUL
        PUSH 16
        SHR
        ADD                     ; x = 120 + cos*90
        PUSH 120
        LDL 2
        SYS SIN
        PUSH 90
        MUL
        PUSH 16
        SHR
        ADD                     ; y = 120 + sin*90
        PUSH 7
        LDL 1
        INC
        SYS FCIRCLE
        LDL 1
        INC
        DUP
        STL 1
        PUSH 12
        LT
        JNZ dot
        PUSH 120
        PUSH 120
        PUSH 5
        PUSH 15
        SYS FCIRCLE
        SYS FLIP
        LDL 0
        PUSH 3
        ADD
        STL 0
        PUSH 16
        SYS SLEEP
        JMP frame
quit:   HALT
