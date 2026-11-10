; lines.asm - starburst drawn with LINE and the 256-colour palette. Esc / close quits.
        .equ WIN,   10
        .equ LINE,  22
        .equ FLIP,  26
        .equ POLL,  30
        .equ SLEEP, 32
main:   ENTER 8                 ; 0=i 1=j  4..7=event
        PUSH 320
        PUSH 240
        SYS WIN
        DROP
        PUSH 0
        STL 0
top:    PUSH 160
        PUSH 120
        LDL 0
        PUSH 0
        LDL 0
        PUSH 3
        SHR
        PUSH 16
        ADD
        SYS LINE                ; to the top edge
        PUSH 160
        PUSH 120
        LDL 0
        PUSH 239
        LDL 0
        PUSH 3
        SHR
        PUSH 60
        ADD
        SYS LINE                ; to the bottom edge
        LDL 0
        PUSH 8
        ADD
        DUP
        STL 0
        PUSH 320
        LT
        JNZ top
        PUSH 0
        STL 1
side:   PUSH 160
        PUSH 120
        PUSH 0
        LDL 1
        LDL 1
        PUSH 3
        SHR
        PUSH 100
        ADD
        SYS LINE                ; to the left edge
        PUSH 160
        PUSH 120
        PUSH 319
        LDL 1
        LDL 1
        PUSH 3
        SHR
        PUSH 140
        ADD
        SYS LINE                ; to the right edge
        LDL 1
        PUSH 8
        ADD
        DUP
        STL 1
        PUSH 240
        LT
        JNZ side
        SYS FLIP

wait:   SYS POLL
        STL 7
        STL 6
        STL 5
        STL 4
        LDL 5
        PUSH 1
        EQ
        JNZ done
        LDL 5
        PUSH 6
        EQ
        JNZ done
        LDL 5
        PUSH 2
        EQ
        JZ nap
        LDL 6
        PUSH 27
        EQ
        JNZ done
nap:    PUSH 10
        SYS SLEEP
        JMP wait
done:   HALT
