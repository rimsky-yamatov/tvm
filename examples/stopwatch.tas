; stopwatch.asm - analog stopwatch: hands from TICK, positions from SIN / COS. Esc / close quits.
        .equ WIN,     10
        .equ CIRCLE,  16
        .equ FCIRCLE, 17
        .equ LINE,    22
        .equ CLEAR,   24
        .equ FLIP,    26
        .equ POLL,    30
        .equ TICK,    31
        .equ SLEEP,   32
        .equ SIN,     40
        .equ COS,     41
main:   ENTER 8                 ; 0=ms 1=k 2=angle  4..7=event
        PUSH 200
        PUSH 200
        SYS WIN
        DROP
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
        PUSH 100
        PUSH 100
        PUSH 92
        PUSH 7
        SYS CIRCLE
        PUSH 0
        STL 1
tick:   LDL 1
        PUSH 30
        MUL
        STL 2
        LDL 2
        PUSH 80
        CALL px
        LDL 2
        PUSH 80
        CALL py
        LDL 2
        PUSH 90
        CALL px
        LDL 2
        PUSH 90
        CALL py
        PUSH 15
        SYS LINE                ; hour mark
        LDL 1
        INC
        DUP
        STL 1
        PUSH 12
        LT
        JNZ tick
        SYS TICK
        STL 0
        LDL 0                   ; second hand: 6 degrees per second
        PUSH 6
        MUL
        PUSH 1000
        DIV
        STL 2
        PUSH 100
        PUSH 100
        LDL 2
        PUSH 75
        CALL px
        LDL 2
        PUSH 75
        CALL py
        PUSH 9
        SYS LINE
        LDL 0                   ; minute hand: 6 degrees per minute
        PUSH 10000
        DIV
        STL 2
        PUSH 100
        PUSH 100
        LDL 2
        PUSH 55
        CALL px
        LDL 2
        PUSH 55
        CALL py
        PUSH 10
        SYS LINE
        PUSH 100
        PUSH 100
        PUSH 3
        PUSH 15
        SYS FCIRCLE
        SYS FLIP
        PUSH 16
        SYS SLEEP
        JMP frame
quit:   HALT

px:                             ; px(angle, r) = 100 + sin(angle) * r
        PUSH 100
        LDL -2
        SYS SIN
        LDL -1
        MUL
        PUSH 16
        SHR
        ADD
        RETV 2
py:                             ; py(angle, r) = 100 - cos(angle) * r
        PUSH 100
        LDL -2
        SYS COS
        LDL -1
        MUL
        PUSH 16
        SHR
        SUB
        RETV 2
