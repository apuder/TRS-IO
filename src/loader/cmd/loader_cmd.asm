
    org 05100h
    ld hl,0ffffh		;

    ld a,16			; Enable I/O bus on MIII
    out (236),a
    ld a,3
    out (31),a
    out (31),a
    ld a,l
    out (31),a
    ld a,h
    out (31),a

    ld a,(0125h)		; Are we running on a MIII?
    sub 'I'
    jr z,wait_m3
	
    ld hl,037e0h
wait_m1:
    ld a,(hl)
    and 020h
    jr nz,wait_m1
    jr cont

wait_m3:
    in a,(0e0h)
    and 8
    jr nz,wait_m3

cont:
    in a,(31)
    in a,(31)
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
    ld hl,03c00h
    ld (hl),a
    jr $
