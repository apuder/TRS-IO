        org     4FFFh

        defb    254
entry   ld      a,0FFh
        ld      hl,wtmsg        ; display waiting message
        ld      de,3C00h        ; start of display
        ld      bc,wtmsgend-wtmsg
        ldir
        ex      de,hl
        ld      de,0            ; init counter
lp00    ld      a,(3840h)       ; check keyboard
        and     80h             ; <SPACE> pressed ?
        ret     nz              ; return if so
        ld      a,(entry+1)
        out     (0C5h),a
        in      a,(0C4h)        ; should be 254
        in      a,(0C4h)        ; get first byte
        ld      b,a             ; compare with ours
        ld      a,(entry)
        cp      b
        jr      nz,boot         ; if different then boot frehd

        dec     de              ; bump counter
        ld      a,d             ; check for time to update disp
        and     07h
        or      e
        jr      nz,lp00
        ld      a,d             ; output . or spc
        and     80h
        ld      a,'.'
        jr      nz,lp10
        ld      a,' '
lp10    ld      (hl),a
        inc     hl              ; bump char position
        ld      a,d             ; wrap after 16 chars
        and     78h
        jr      nz,lp00
        ld      l,wtmsgend-wtmsg
        jr      lp00

boot    ld      hl,entry       ; loader start
        ld      (hl),b         ; sto byte read
        inc     hl             ; bump
        ld      bc,0FFC4h      ; load 255 more from C4h
        defb    11h            ; ld de,nnnn
        inir                   ; sto inir opcode just before loader
        ld      (entry-2),de
        jp      entry-2        ; load and run loader

wtmsg   defb    'WAITING FOR TRS-IO READY'
wtmsgend

        defb    0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh
        defb    0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh
        defb    0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh
        defb    0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh
        defb    0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh
        defb    0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh
        defb    0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh
        defb    0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh
        defb    0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh
        defb    0FFh,0FFh,0FFh

        end     entry
