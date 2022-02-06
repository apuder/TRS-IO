; XRAY debug stub
; This stub is location independent and will be injected whenever
; execution hits a breakpoint. This stub will push the context onto
; a stack starting at address 0FFFFh without modifying any registers

    org 0000h

breakpoint:
    di
    ld (reg_sp),sp
    ld sp,reg_sp
    call 0ff00h    ; peek(0ff00h) == 0c9h == RET
ret_label:
diff: equ ret_label-breakpoint
    dec sp         ; Restore RET address that is still in memory
    dec sp

    push iy
    push ix
    exx
    ex af,af'
    push hl
    push de
    push bc
    push af
    exx
    ex af,af'
    push hl
    push de
    push bc
    push af
    ld sp,(reg_sp)
    jr breakpoint

    org 0fffeh

reg_sp:

