; win2.asm - two windows. A white square follows the mouse in whichever window it is over.
; Esc or closing any window quits.
        .equ WIN,    10
        .equ WSEL,   11
        .equ WTITLE, 13
        .equ TEXT,   23
        .equ RECT,   21
        .equ CLEAR,  24
        .equ FLIP,   26
        .equ POLL,   30
        .equ SLEEP,  32

main:   ENTER 6                 ; locals: 0=winA 1=winB 2=ev.win 3=ev.type 4=ev.a 5=ev.b
        PUSH 200
        PUSH 150
        SYS WIN
        STL 0
        PUSH ta
        SYS WTITLE
        PUSH 200
        PUSH 150
        SYS WIN
        STL 1
        PUSH tb
        SYS WTITLE
        LDL 0
        PUSH -100
        PUSH -100
        CALL redraw
        LDL 1
        PUSH -100
        PUSH -100
        CALL redraw

loop:   SYS POLL                ; -> win type a b
        STL 5
        STL 4
        STL 3
        STL 2
        LDL 3
        JZ idle
        LDL 3
        PUSH 1                  ; quit
        EQ
        JNZ quit
        LDL 3
        PUSH 6                  ; window close
        EQ
        JNZ quit
        LDL 3
        PUSH 2                  ; key
        EQ
        JZ chkmv
        LDL 4
        PUSH 27                 ; Esc
        EQ
        JNZ quit
        JMP loop
chkmv:  LDL 3
        PUSH 3                  ; mouse move
        EQ
        JZ loop
        LDL 2
        LDL 4
        LDL 5
        CALL redraw
        JMP loop
idle:   PUSH 10
        SYS SLEEP
        JMP loop
quit:   HALT

redraw:                         ; redraw(win, x, y)
        LDL -3
        SYS WSEL
        LDL -3
        PUSH bg
        ADD
        LD8
        SYS CLEAR
        LDL -2
        PUSH 8
        SUB
        LDL -1
        PUSH 8
        SUB
        PUSH 16
        PUSH 16
        PUSH 15
        SYS RECT
        PUSH 4
        PUSH 4
        PUSH label
        PUSH 11
        SYS TEXT
        SYS FLIP
        RET 3

bg:     .byte 4, 2, 5, 6, 3, 1, 8, 9
ta:     .str "Window A"
tb:     .str "Window B"
label:  .str "tvm: move the mouse"
