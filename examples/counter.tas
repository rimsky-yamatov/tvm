; counter.asm - two windows: seconds since start / frame counter (itoa in assembly)
        .equ WIN,    10
        .equ WSEL,   11
        .equ WTITLE, 13
        .equ TEXT,   23
        .equ CLEAR,  24
        .equ FLIP,   26
        .equ POLL,   30
        .equ TICK,   31
        .equ SLEEP,  32
main:   ENTER 8                 ; 0=winA 1=winB 2=frames  4..7=event
        PUSH 120
        PUSH 40
        SYS WIN
        STL 0
        PUSH ta
        SYS WTITLE
        PUSH 120
        PUSH 40
        SYS WIN
        STL 1
        PUSH tb
        SYS WTITLE

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
upd:    SYS TICK
        PUSH 1000
        DIV
        CALL fmt5
        LDL 0
        CALL show
        LDL 2
        CALL fmt5
        LDL 1
        CALL show
        LDL 2
        INC
        STL 2
        PUSH 16
        SYS SLEEP
        JMP frame
quit:   HALT

show:                           ; show(win): draw buf into that window
        LDL -1
        SYS WSEL
        PUSH 4
        SYS CLEAR
        PUSH 45
        PUSH 16
        PUSH buf
        PUSH 15
        SYS TEXT
        SYS FLIP
        RET 1

fmt5:                           ; fmt5(n): write 5 decimal digits into buf
        ENTER 1
        PUSH 4
        STL 0
f1:     LDL -1
        PUSH 10
        MOD
        PUSH 48
        ADD
        LDL 0
        PUSH buf
        ADD
        ST8
        LDL -1
        PUSH 10
        DIV
        STL -1
        LDL 0
        DEC
        DUP
        STL 0
        PUSH 0
        GE
        JNZ f1
        RET 1

buf:    .str "00000"
ta:     .str "seconds"
tb:     .str "frames"
