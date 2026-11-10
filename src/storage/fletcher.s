;  ETCH -- A Generative Acid Machine -- by Synth Centric
;  fletcher_asm: Fletcher-16 (mod 255 via end-around carry) over fl_n bytes
;  at fl_ptr.  Result in fl_sum (lo = sum1, hi = sum2), each normalised so
;  that 255 == 0.  ~60 cycles per byte.
	.module fletcher
	.area _DATA
_fl_ptr::	.ds 2
_fl_n::	.ds 2
_fl_sum::	.ds 2
	.area _HOME
_fletcher_asm::
	ld	hl, #_fl_n
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	ld	hl, #_fl_ptr
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	ld	bc, #0
	ld	a, d
	or	a, e
	jr	z, 2$
1$:	ld	a, (hl+)
	add	a, c
	adc	a, #0
	ld	c, a
	add	a, b
	adc	a, #0
	ld	b, a
	dec	de
	ld	a, d
	or	a, e
	jr	nz, 1$
2$:	ld	a, c
	inc	a
	jr	nz, 3$
	ld	c, a
3$:	ld	a, b
	inc	a
	jr	nz, 4$
	ld	b, a
4$:	ld	hl, #_fl_sum
	ld	a, c
	ld	(hl+), a
	ld	(hl), b
	ret
