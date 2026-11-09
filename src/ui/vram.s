;  ETCH -- A Generative Acid Machine -- by Synth Centric
;  vram_copy_asm: copy vr_n bytes (1..256, 0 = 256) from vr_src to VRAM vr_dst.
;  Each byte is written with interrupts disabled immediately after checking
;  that the PPU is in mode 0 or 1, so a timer interrupt can never land between
;  the check and the write.  Interrupts are off for at most one mode-2/3
;  period (< 100 us), which only delays -- never drops -- a sequencer tick.
	.module vram
	.area _DATA
_vr_src::	.ds 2
_vr_dst::	.ds 2
_vr_n::	.ds 1
	.area _HOME
_vram_copy_asm::
	ld	hl, #_vr_src
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	ld	hl, #_vr_dst
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	ld	a, (#_vr_n)
	ld	b, a
1$:	ld	a, (de)
	inc	de
	ld	c, a
	di
2$:	ldh	a, (#0x41)
	and	a, #0x02
	jr	nz, 2$
	ld	a, c
	ld	(hl+), a
	ei
	dec	b
	jr	nz, 1$
	ret
