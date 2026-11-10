; sprite.asm - BLIT a transparent 8x8 sprite at the mouse (MOUSE state).
; Hold the left button to paint with it, the right button clears.
        .equ WIN,   10
        .equ BLIT,  15
        .equ CLEAR, 24
        .equ FLIP,  26
        .equ POLL,  30
        .equ SLEEP, 32
        .equ MOUSE, 34
main:   ENTER 8                 ; 1=x 2=y 3=buttons  4..7=event
        PUSH 240
        PUSH 160
        SYS WIN
        DROP
        PUSH 4
        SYS CLEAR
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
        JZ upd
        LDL 6
        PUSH 27
        EQ
        JNZ quit
upd:    SYS MOUSE               ; -> win x y buttons
        STL 3
        STL 2
        STL 1
        DROP
        LDL 3
        PUSH 4
        AND
        JZ stamp
        PUSH 4
        SYS CLEAR
stamp:  LDL 3
        PUSH 1
        AND
        JZ show
        LDL 1
        PUSH 4
        SUB
        LDL 2
        PUSH 4
        SUB
        PUSH 8
        PUSH 8
        PUSH sprite
        PUSH 255                ; transparent colour
        SYS BLIT
show:   SYS FLIP
        PUSH 8
        SYS SLEEP
        JMP frame
quit:   HALT

sprite: .byte 255,255,  8,  8,  8,  8,255,255
        .byte 255,  8, 11, 11, 11, 11,  8,255
        .byte   8, 11,  0, 11, 11,  0, 11,  8
        .byte   8, 11, 11, 11, 11, 11, 11,  8
        .byte   8, 11,  0, 11, 11,  0, 11,  8
        .byte   8, 11, 11,  0,  0, 11, 11,  8
        .byte 255,  8, 11, 11, 11, 11,  8,255
        .byte 255,255,  8,  8,  8,  8,255,255
