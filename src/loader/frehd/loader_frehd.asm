
    org 4200h
    ld a,1
    out (197),a
    in a,(196)
    cp 254
    jp nz,0075h
    ld b,0
    ld hl,20480
LOOP:
    in a,(196)
    ld (hl),a
    inc hl
    djnz LOOP
    jp 20480

