; music.asm - plays a C major scale with BEEP (needs an audio device)
        .equ BEEP,  50
        .equ SLEEP, 32
main:   ENTER 1
        PUSH 0
        STL 0
next:   LDL 0
        PUSH 2
        SHL
        PUSH notes
        ADD
        LD                      ; frequency in Hz
        PUSH 250
        SYS BEEP
        PUSH 300
        SYS SLEEP
        LDL 0
        INC
        DUP
        STL 0
        PUSH 8
        LT
        JNZ next
        HALT
notes:  .word 262, 294, 330, 349, 392, 440, 494, 523
