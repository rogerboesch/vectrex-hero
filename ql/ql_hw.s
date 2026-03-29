;
; ql_hw.s — QDOS hardware abstraction (68K assembly)
;
; C-callable functions for QL-specific operations:
;   void ql_init(void)        — set Mode 8, open console channel
;   void ql_cleanup(void)     — restore mode, close channel
;   void ql_read_keys(void)   — poll keyboard, set key_* globals
;   uint16_t ql_get_ticks(void) — read QDOS 50Hz frame counter
;   void ql_beep(uint8_t pitch, uint8_t duration)
;
; vasm68k_mot syntax (Motorola)
;

        section CODE

; =====================================================================
; ql_init — Set display to Mode 8, clear screen
; =====================================================================
        xdef    _ql_init
_ql_init:
        ; Set Mode 8 (256x256, 8 colors)
        moveq   #$10,d0         ; MT.DMODE
        moveq   #8,d1           ; Mode 8
        moveq   #-1,d2          ; don't change display type
        trap    #1

        ; Clear screen memory to black
        move.l  #$20000,a0      ; screen base
        move.w  #(32768/4)-1,d0 ; longword count - 1
.clrscr:
        clr.l   (a0)+
        dbf     d0,.clrscr

        ; Open a console channel for keyboard input
        ; trap #2: d0=open_type, d1=job_id, a0=name
        lea     con_def(pc),a0  ; channel name (PC-relative)
        moveq   #-1,d1          ; current job
        moveq   #0,d3           ; open type: old exclusive
        moveq   #1,d0           ; IO.OPEN (open type=1 old shared)
        trap    #2
        tst.l   d0
        bne.s   .no_con
        move.l  a0,_con_id      ; save channel ID
.no_con:
        rts

; Console channel definition (QDOS string: word length + chars)
con_def:
        dc.w    4               ; name length
        dc.b    "con_"
        even

; =====================================================================
; ql_cleanup — Close console channel
; =====================================================================
        xdef    _ql_cleanup
_ql_cleanup:
        move.l  _con_id,a0
        moveq   #2,d0           ; IO.CLOSE
        trap    #2
        rts

; =====================================================================
; ql_read_keys — Non-blocking keyboard poll
;
; Uses IO.FBYTE on the console channel to read pending keystrokes.
; Sets the key_* global variables (C-accessible).
; =====================================================================
        xdef    _ql_read_keys
_ql_read_keys:
        ; Clear all key states
        clr.b   _key_left
        clr.b   _key_right
        clr.b   _key_up
        clr.b   _key_down
        clr.b   _key_space_pressed
        clr.b   _key_d_pressed
        clr.b   _key_enter_pressed

        ; Read pending keys (non-blocking)
        move.l  _con_id,a0
        moveq   #0,d3           ; timeout = 0 (non-blocking)

.readloop:
        moveq   #1,d0           ; IO.FBYTE
        move.w  #0,d2           ; fetch 1 byte
        trap    #3
        tst.l   d0              ; error? (e.g., no key pending)
        bne     .done

        ; d1.b = keycode
        ; Check each key
        cmpi.b  #'q',d1
        beq     .key_up
        cmpi.b  #'Q',d1
        beq     .key_up
        cmpi.b  #$D0,d1         ; cursor up
        beq     .key_up

        cmpi.b  #'a',d1
        beq     .key_down
        cmpi.b  #'A',d1
        beq     .key_down
        cmpi.b  #$D8,d1         ; cursor down
        beq     .key_down

        cmpi.b  #'o',d1
        beq     .key_left
        cmpi.b  #'O',d1
        beq     .key_left
        cmpi.b  #$C0,d1         ; cursor left
        beq     .key_left

        cmpi.b  #'p',d1
        beq     .key_right
        cmpi.b  #'P',d1
        beq     .key_right
        cmpi.b  #$C8,d1         ; cursor right
        beq     .key_right

        cmpi.b  #' ',d1
        beq     .key_space

        cmpi.b  #'d',d1
        beq     .key_d
        cmpi.b  #'D',d1
        beq     .key_d

        cmpi.b  #$0A,d1         ; enter
        beq     .key_enter

        cmpi.b  #$1B,d1         ; ESC = quit
        beq     .key_esc

        bra     .readloop       ; unknown key, read next

.key_up:    move.b  #1,_key_up
            bra     .readloop
.key_down:  move.b  #1,_key_down
            bra     .readloop
.key_left:  move.b  #1,_key_left
            bra     .readloop
.key_right: move.b  #1,_key_right
            bra     .readloop
.key_space: move.b  #1,_key_space_pressed
            move.b  #1,_key_enter_pressed   ; space also acts as confirm
            bra     .readloop
.key_d:     move.b  #1,_key_d_pressed
            bra     .readloop
.key_enter: move.b  #1,_key_enter_pressed
            bra     .readloop
.key_esc:
        ; ESC pressed — terminate job
        moveq   #-1,d1          ; current job (d1=-1)
        moveq   #0,d3           ; return code 0
        moveq   #5,d0           ; MT.FRJOB ($05)
        trap    #1
.done:
        rts

; =====================================================================
; ql_get_ticks — Read QDOS 50Hz frame counter
;
; Returns uint16_t tick count in d0
; The QL system variable SV.RAND at offset $32 from sys base
; increments at 50Hz and can serve as a frame counter.
; Alternatively use MT.RCLCK.
; =====================================================================
; ql_get_ticks — read QDOS clock (returns d0.l = seconds since epoch)
        xdef    _ql_get_ticks
_ql_get_ticks:
        moveq   #$13,d0         ; MT.RCLCK — read clock
        trap    #1
        move.l  d1,d0           ; return value in d0 (seconds)
        rts

; ql_wait_frame — suspend job for ~1 frame (use instead of busy-wait)
; void ql_wait_frame(void)
        xdef    _ql_wait_frame
_ql_wait_frame:
        moveq   #-1,d1          ; current job
        moveq   #3,d3           ; timeout = 3 ticks (~60ms at 50Hz)
        moveq   #$08,d0         ; MT.SUSJB ($08) — suspend job
        trap    #1
        rts

; =====================================================================
; ql_alloc — Allocate memory from QDOS common heap
;
; void *ql_alloc(uint32_t size)
; size on stack (vbcc convention)
; Returns pointer in d0 (or 0 on failure)
; =====================================================================
        xdef    _ql_alloc
_ql_alloc:
        move.l  4(sp),d1        ; size (first arg on stack)
        moveq   #$18,d0         ; MT.ALCHP — allocate common heap
        moveq   #-1,d2          ; current job
        trap    #1
        tst.l   d0
        bne.s   .alloc_fail
        move.l  a0,d0           ; return pointer
        rts
.alloc_fail:
        moveq   #0,d0           ; return NULL
        rts

; =====================================================================
; ql_beep — Simple beep via IPC
;
; void ql_beep(uint8_t pitch, uint8_t duration)
; pitch in 4(sp), duration in 6(sp) (vbcc calling convention: args on stack)
; =====================================================================
        xdef    _ql_beep
_ql_beep:
        ; MT.IPCOM: send command to IPC
        ; For a beep: send bytes to IPC sound register
        ; This is complex in practice; stub for now
        rts

; =====================================================================
; Data section — C-accessible globals
; =====================================================================
        section DATA,data

        xdef    _con_id
_con_id:
        dc.l    0               ; console channel ID

; Key state globals (referenced from C code)
        xdef    _key_left
        xdef    _key_right
        xdef    _key_up
        xdef    _key_down
        xdef    _key_space_pressed
        xdef    _key_d_pressed
        xdef    _key_enter_pressed

_key_left:          dc.b    0
_key_right:         dc.b    0
_key_up:            dc.b    0
_key_down:          dc.b    0
_key_space_pressed: dc.b    0
_key_d_pressed:     dc.b    0
_key_enter_pressed: dc.b    0
        even
