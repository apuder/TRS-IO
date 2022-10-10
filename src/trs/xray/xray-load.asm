; XRAY loader stub
; This stub restores a context stored in high memory
; Used when migrating a state to a machine

    org 0000h

loop:
    di
    ld sp,0xffe8
    pop af
    pop bc
    pop de
    pop hl
    ex af,af'
    exx
    pop af
    pop bc
    pop de
    pop hl
    ex af,af'
    exx
    pop ix
    pop iy
    ld sp,(0xfffe)
    jr loop
