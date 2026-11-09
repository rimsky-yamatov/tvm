; hello.asm - Hello, World!
        PUSH msg
        CALL puts
        HALT

puts:                       ; puts(ptr): args are at LDL -1 ...
putsl:  LDL -1
        LD8
        DUP
        JZ putse            ; pops the copy, ch stays on the stack
        SYS 0               ; PUTC(ch)
        LDL -1
        INC
        STL -1
        JMP putsl
putse:  DROP
        RET 1               ; drop 1 argument

msg:    .str "Hello, World!\n"
