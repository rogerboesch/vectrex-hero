        section CODE
        xdef    _start
        xref    _main

_start:
        jsr     _main

        ; Exit: MT.FRJOB
        moveq   #-1,d1
        moveq   #0,d3
        moveq   #5,d0
        trap    #1
        bra.s   *
