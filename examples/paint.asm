; paint.asm - drag the mouse to draw. Keys 1-9 pick a colour, c clears, Esc quits.
        .equ WIN,   10
        .equ RECT,  21
        .equ TEXT,  23
        .equ CLEAR, 24
        .equ FLIP,  26
        .equ POLL,  30
        .equ SLEEP, 32
main:   ENTER 8                 ; 0=pen down 1=colour  4..7=event
        PUSH 320
        PUSH 240
        SYS WIN
        DROP
        PUSH 15
        STL 1
        PUSH 4
        PUSH 4
        PUSH help
        PUSH 8
        SYS TEXT
        SYS FLIP

loop:   SYS POLL
        STL 7
        STL 6
        STL 5
        STL 4
        LDL 5
        JZ idle
        LDL 5
        PUSH 1
        EQ
        JNZ quit
        LDL 5
        PUSH 6
        EQ
        JNZ quit
        LDL 5
        PUSH 4                  ; mouse down
        EQ
        JZ c5
        PUSH 1
        STL 0
        JMP dot
c5:     LDL 5
        PUSH 5                  ; mouse up
        EQ
        JZ c3
        PUSH 0
        STL 0
        JMP loop
c3:     LDL 5
        PUSH 3                  ; mouse move
        EQ
        JZ c2
        LDL 0
        JZ loop
dot:    LDL 6
        PUSH 1
        SUB
        LDL 7
        PUSH 1
        SUB
        PUSH 3
        PUSH 3
        LDL 1
        SYS RECT
        SYS FLIP
        JMP loop
c2:     LDL 5
        PUSH 2                  ; key
        EQ
        JZ loop
        LDL 7
        JZ loop                 ; ignore key-up
        LDL 6
        PUSH 27
        EQ
        JNZ quit
        LDL 6
        PUSH 99                 ; 'c'
        EQ
        JZ pick
        PUSH 0
        SYS CLEAR
        SYS FLIP
        JMP loop
pick:   LDL 6
        PUSH 49
        LT
        JNZ loop
        LDL 6
        PUSH 57
        GT
        JNZ loop
        LDL 6
        PUSH 48
        SUB
        STL 1
        JMP loop
idle:   PUSH 8
        SYS SLEEP
        JMP loop
quit:   HALT

help:   .str "drag: draw   1-9: colour   c: clear   Esc: quit"
