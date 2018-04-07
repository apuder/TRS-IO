
CLS: equ 01c9h

    org 4300h
    call CLS
    ld a,56
    out (236),a
    ld a,1
    out (31),a

LOOP:
    in a,(31)
    ld e,a
    in a,(31)
    ld b,0
    ld c,a
    dec e
    jr nz,check_entry
    or a
    jr z,need_offset
    dec a
    jr z,need_offset
    dec a
    jr z,need_offset
    jr no_offset
need_offset:
    ld hl,256
    add hl,bc
    ld c,l
    ld b,h
no_offset:
    dec bc
    dec bc
    in a,(31)
    ld l,a
    in a,(31)
    ld h,a
load_cmd:
    ld a,b
    or c
    jr z,LOOP
    in a,(31)
    ld (hl),a
    inc hl
    dec bc
    jr load_cmd
check_entry:
    dec e
    jr nz,check_skip
    in a,(31)
    ld l,a
    in a,(31)
    ld h,a
    jp (hl)
check_skip:
    dec e
    dec e
    dec e
    jr nz,bad_cmd
skip:
    ld a,b
    or c
    jr z,LOOP
    in a,(31)
    dec bc
    jr skip
bad_cmd:
    ld a,'*'
    call 033Ah
endless_loop:
    jr endless_loop

