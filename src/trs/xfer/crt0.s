;--------------------------------------------------------------------------
;  crt0.s - Generic crt0.s for a Z80
;
;  Copyright (C) 2000, Michael Hope
;
;  This library is free software; you can redistribute it and/or modify it
;  under the terms of the GNU General Public License as published by the
;  Free Software Foundation; either version 2, or (at your option) any
;  later version.
;
;  This library is distributed in the hope that it will be useful,
;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
;  GNU General Public License for more details.
;
;  You should have received a copy of the GNU General Public License 
;  along with this library; see the file COPYING. If not, write to the
;  Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
;   MA 02110-1301, USA.
;
;  As a special exception, if you link this library with other files,
;  some of which are compiled with SDCC, to produce an executable,
;  this library does not by itself cause the resulting executable to
;  be covered by the GNU General Public License. This exception does
;  not however invalidate any other reasons why the executable file
;   might be covered by the GNU General Public License.
;--------------------------------------------------------------------------

	.module crt0
	.globl	_main
	.globl	_init_trs_lib
	.globl	_exit_trs_lib
  .globl  exit
  .globl  abort
  .globl  error
	.globl  l__DATA
	.globl  s__DATA
	.globl  l__INITIALIZER
	.globl  s__INITIALIZER
	.globl  s__INITIALIZED

init:
	ex	de,hl  ; Move args pointer to de
	;; For M4, turn on MIII memory map. NOP on a M1/III
	xor	a
	out	(0x84), a

	;; Determine the highest memory address for the stack
	ld	(sp_save),sp
	ld	a,(0x125)
	sub	#'I'
	jr	z,is_m3
	ld	hl,(0x4049)	; Highest memory for the M1
	jr	cont
is_m3:
	ld	hl,(0x4411)	; Highest memory for the M3
cont:
	inc	hl
	ld	a,#0x80
	and	h
	jr	nz,cont1
	ld	hl,#0 ; SP was pointing below 0x8000
cont1:
	ld	sp,hl

	;; Initialise global variables
	push	de
	call	gsinit
	call	_init_trs_lib
	pop	hl
	push	hl
	pop	de
cont2:        ; Convert \n to \0
	ld	a,(de)
	cp	#0x0d
	jr	z,cont3
	inc	de
	jr	cont2
cont3:
	xor	a
	ld	(de),a
  push de      ; Remember location
	call	_main
  pop hl
  ld (hl),#0x0d
  push de      ; Remember return code from main()
	call	_exit_trs_lib
	pop	hl
	ld	sp,(sp_save)
	ld	a,l
	or	h
	jr	z,ok
	jp	abort
ok:	jp	exit

sp_save:
	.dw	0

	;; Ordering of segments for the linker.
	.area	_HOME
	.area	_CODE
	.area	_INITIALIZER
	.area   _GSINIT
	.area   _GSFINAL

	.area	_DATA
	.area	_INITIALIZED
	.area	_BSEG
	.area   _BSS
	.area   _HEAP

	.area   _CODE

	.area   _GSINIT
gsinit::

	; Default-initialized global variables.
        ld      bc, #l__DATA
        ld      a, b
        or      a, c
        jr      Z, zeroed_data
        ld      hl, #s__DATA
        ld      (hl), #0x00
        dec     bc
        ld      a, b
        or      a, c
        jr      Z, zeroed_data
        ld      e, l
        ld      d, h
        inc     de
        ldir
zeroed_data:

	; Explicitly initialized global variables.
	ld	bc, #l__INITIALIZER
	ld	a, b
	or	a, c
	jr	Z, gsinit_next
	ld	de, #s__INITIALIZED
	ld	hl, #s__INITIALIZER
	ldir

gsinit_next:

	.area   _GSFINAL
	ret

