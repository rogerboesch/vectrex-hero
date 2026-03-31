; =====================================================================
; crt0.s — C runtime startup for Sinclair QL (QDOS)
;
; This is the entry point for the QDOS executable. The tos2ql.py tool
; prepends a self-relocating loader stub that patches absolute addresses,
; then jumps here.
;
; Responsibilities:
;   1. Call _main (the C entry point in main.c)
;   2. On return (or fall-through), terminate the job via MT.FRJOB
;
; QDOS job termination:
;   MT.FRJOB (trap #1, d0=$05): Force-remove job
;   d1 = job ID (-1 = current job)
;   d3 = error code (0 = no error)
;
; The "bra.s *" at the end is a safety halt — an infinite loop that
; should never execute since MT.FRJOB kills the job. It prevents
; runaway execution if the trap somehow fails.
;
; Build: vasm68k_mot -Fvobj -m68000 → linked first in vlink order
; =====================================================================

        section CODE
        xdef    _start
        xref    _main

_start:
        jsr     _main           ; call C main() — game loop runs here

        ; main() returned (shouldn't happen normally) — exit cleanly
        moveq   #-1,d1          ; d1 = -1 → current job
        moveq   #0,d3           ; d3 = 0  → no error code
        moveq   #5,d0           ; MT.FRJOB ($05) — force remove job
        trap    #1
        bra.s   *               ; safety halt (never reached)
