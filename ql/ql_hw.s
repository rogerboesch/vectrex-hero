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
; asm_blit_sprite — Fast sprite blit to buffer in 68K assembly
;
; void asm_blit_sprite(uint8_t *buf, const Sprite *spr,
;                      int16_t sx, int16_t sy)
;
; vbcc: args on stack. Sprite struct: byte w, byte h, long data_ptr
; Nibble-packed sprite data → Mode 8 bit-plane screen memory.
;
; Uses a 32-byte lookup table: for each (color 0-7, pos 0-3),
; stores the even-byte and odd-byte bits to OR in.
; =====================================================================
        xdef    _asm_blit_sprite
_asm_blit_sprite:
        movem.l d2-d7/a2-a4,-(sp)  ; 9 regs = 36 bytes
        move.l  40(sp),a4           ; a4 = buf
        move.l  44(sp),a0           ; a0 = spr
        move.w  48(sp),d4           ; d4 = sx
        move.w  50(sp),d5           ; d5 = sy

        moveq   #0,d6
        move.b  (a0),d6             ; d6 = w
        moveq   #0,d7
        move.b  1(a0),d7            ; d7 = h
        move.l  2(a0),a2            ; a2 = data
        lsr.w   #1,d6               ; d6 = wb

        moveq   #0,d2               ; d2 = row
.brow:  cmp.w   d7,d2
        bge     .bdone

        move.w  d5,d0
        add.w   d2,d0               ; d0 = py
        bmi     .bnrow
        cmpi.w  #256,d0
        bge     .bnrow

        ; a3 = buf + py << 7
        move.w  d0,d3
        ext.l   d3
        lsl.l   #7,d3
        move.l  a4,a3
        adda.l  d3,a3

        ; a0 = data + row * wb
        move.l  a2,a0
        move.w  d2,d0
        mulu    d6,d0
        adda.l  d0,a0

        moveq   #0,d3               ; d3 = col
.bcol:  cmp.w   d6,d3
        bge     .bnrow

        moveq   #0,d0
        move.b  (a0)+,d0
        beq     .bncol              ; skip if both transparent

        move.l  a0,-(sp)            ; SAVE src pointer
        move.w  d2,-(sp)            ; SAVE row counter
        move.w  d0,d1               ; d1 = save byte
        move.w  d3,d0
        add.w   d0,d0
        add.w   d4,d0               ; d0 = pixel_x for left pixel

        ; --- Left pixel (hi nibble) ---
        move.w  d1,d2
        lsr.w   #4,d2
        andi.w  #7,d2               ; d2 = hi color
        beq.s   .bskiphi
        tst.w   d0
        bmi.s   .bskiphi
        cmpi.w  #256,d0
        bge.s   .bskiphi
        move.w  d0,-(sp)            ; save pixel_x (bpx clobbers d0)
        bsr.s   .bpx
        move.w  (sp)+,d0            ; restore pixel_x
.bskiphi:

        ; --- Right pixel (lo nibble) ---
        addq.w  #1,d0               ; pixel_x + 1
        move.w  d1,d2
        andi.w  #7,d2               ; d2 = lo color
        beq.s   .bskiplo
        cmpi.w  #256,d0
        bge.s   .bskiplo
        bsr.s   .bpx
.bskiplo:

        move.w  (sp)+,d2            ; RESTORE row counter
        move.l  (sp)+,a0            ; RESTORE src pointer

.bncol:
        addq.w  #1,d3
        bra     .bcol

.bnrow: addq.w  #1,d2
        bra     .brow

.bdone: movem.l (sp)+,d2-d7/a2-a4
        rts

; ----- Inline pixel plot -----
; d2 = color (1-7), d0 = x (0-255), a3 = row base
; Clobbers: a0, a1, d0, d2
; Preserves: a0 is NOT preserved — caller must save it!
.bpx:
        ; pair addr = a3 + (x/4)*2
        move.w  d0,-(sp)
        lsr.w   #2,d0
        add.w   d0,d0
        lea     0(a3,d0.w),a1       ; a1 = even byte addr

        ; index = color*4 + (x & 3)
        move.w  (sp)+,d0
        andi.w  #3,d0
        lsl.w   #2,d2               ; color * 4
        add.w   d0,d2               ; d2 = table index

        ; Clear position bits first, then OR new bits
        lea     .pxtab_cl(pc),a0
        move.b  0(a0,d2.w),d0       ; d0 = clear mask
        and.b   d0,(a1)             ; clear even byte position
        and.b   d0,1(a1)            ; clear odd byte position

        lea     .pxtab_ev(pc),a0
        move.b  0(a0,d2.w),d0
        or.b    d0,(a1)             ; set green bits

        lea     .pxtab_od(pc),a0
        move.b  0(a0,d2.w),d0
        or.b    d0,1(a1)            ; set red/flash bits
        rts

; Pixel lookup tables: 8 colors * 4 positions = 32 entries each
; even_bits[color][pos] — bits to OR into green byte
; odd_bits[color][pos]  — bits to OR into red byte
; clear[color][pos]     — bits to AND (clear position first)
;
; Color bits: G=bit2, R=bit1, B=bit0
; Position N: G goes to bit (7-2N), B to bit (7-2N), R to bit (6-2N)
;   pos0: G→bit7, R→bit6 of odd, B→bit7 of odd, clear=0x3F
;   pos1: G→bit5, R→bit4, B→bit5, clear=0xCF
;   pos2: G→bit3, R→bit2, B→bit3, clear=0xF3
;   pos3: G→bit1, R→bit0, B→bit1, clear=0xFC

; Clear mask per position (same for all colors)
.pxtab_cl:
        dc.b    $3F,$3F,$3F,$3F     ; color 0 (never used)
        dc.b    $3F,$CF,$F3,$FC     ; color 1
        dc.b    $3F,$CF,$F3,$FC     ; color 2
        dc.b    $3F,$CF,$F3,$FC     ; color 3
        dc.b    $3F,$CF,$F3,$FC     ; color 4
        dc.b    $3F,$CF,$F3,$FC     ; color 5
        dc.b    $3F,$CF,$F3,$FC     ; color 6
        dc.b    $3F,$CF,$F3,$FC     ; color 7

; Even byte (green plane) bits: G bit of color → high bit of position pair
; G is color bit 2. pos0→bit7, pos1→bit5, pos2→bit3, pos3→bit1
.pxtab_ev:
        dc.b    $00,$00,$00,$00     ; color 0
        dc.b    $00,$00,$00,$00     ; color 1 (G=0)
        dc.b    $00,$00,$00,$00     ; color 2 (G=0)
        dc.b    $00,$00,$00,$00     ; color 3 (G=0)
        dc.b    $80,$20,$08,$02     ; color 4 (G=1)
        dc.b    $80,$20,$08,$02     ; color 5 (G=1)
        dc.b    $80,$20,$08,$02     ; color 6 (G=1)
        dc.b    $80,$20,$08,$02     ; color 7 (G=1)

; Odd byte (red/flash plane): R→low bit, B→high bit of position pair
; R is color bit 1. pos0→bit6, pos1→bit4, pos2→bit2, pos3→bit0
; B is color bit 0. pos0→bit7, pos1→bit5, pos2→bit3, pos3→bit1
.pxtab_od:
        dc.b    $00,$00,$00,$00     ; color 0
        dc.b    $80,$20,$08,$02     ; color 1 (B=1,R=0)
        dc.b    $40,$10,$04,$01     ; color 2 (B=0,R=1)
        dc.b    $C0,$30,$0C,$03     ; color 3 (B=1,R=1)
        dc.b    $00,$00,$00,$00     ; color 4 (B=0,R=0)
        dc.b    $80,$20,$08,$02     ; color 5 (B=1,R=0)
        dc.b    $40,$10,$04,$01     ; color 6 (B=0,R=1)
        dc.b    $C0,$30,$0C,$03     ; color 7 (B=1,R=1)
        even

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
