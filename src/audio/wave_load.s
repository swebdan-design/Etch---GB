;  ETCH -- A Generative Acid Machine -- by Synth Centric
;  wave_load_asm: copy one 16-byte wavetable from its ROM bank through a
;  256-byte transfer LUT (drive/amplitude) into CH3 wave RAM.
;  Inputs (globals set by engine.c): wl_w, wl_r, wl_c (wave/res/cut indices),
;  wl_d, wl_a (drive/amp), wl_bank_w, wl_bank_l, wl_hi (NR34 high bits).
;  wavetable offset = ((w*8 + r) * 32 + c) * 16 ; LUT offset = (d*8 + a) * 256
;  The DAC is switched off (NR30=0) while wave RAM is written -- writing wave
;  RAM or retriggering CH3 while it plays corrupts wave RAM on DMG hardware --
;  then re-enabled and the channel retriggered.  ~900 cycles.
	.module wave_load
	.globl __current_bank
	.globl _etch_wavetables
	.globl _etch_wavelut
	.area _DATA
_wl_tmp::	.ds 16
_wl_w::	.ds 1
_wl_r::	.ds 1
_wl_c::	.ds 1
_wl_d::	.ds 1
_wl_a::	.ds 1
_wl_bank_w::	.ds 1
_wl_bank_l::	.ds 1
_wl_hi::	.ds 1
	.area _HOME
_wave_load_asm::
	ldh	a, (__current_bank)
	push	af
	ld	a, (#_wl_bank_w)
	ldh	(__current_bank), a
	ld	(#0x2000), a
	; hl = (w*8+r)*512 + c*16 + etch_wavetables
	ld	a, (#_wl_w)
	add	a, a
	add	a, a
	add	a, a
	ld	hl, #_wl_r
	or	a, (hl)
	add	a, a		; (w*8+r)*2 = high byte of *512
	ld	h, a
	ld	a, (#_wl_c)
	swap	a		; c*16: low nibble -> high nibble
	ld	l, a
	and	a, #0x0F	; carry part of c*16 (c >= 16)
	add	a, h
	ld	h, a
	ld	a, l
	and	a, #0xF0
	ld	l, a
	ld	de, #_etch_wavetables
	add	hl, de
	ld	de, #_wl_tmp
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (hl+)
	ld	(de), a
	inc	de
	ld	a, (#_wl_bank_l)
	ldh	(__current_bank), a
	ld	(#0x2000), a
	; bc = etch_wavelut + (d*8+a)*256
	ld	a, (#_wl_d)
	add	a, a
	add	a, a
	add	a, a
	ld	hl, #_wl_a
	or	a, (hl)
	ld	b, a
	ld	hl, #_etch_wavelut
	ld	a, l
	ld	c, a
	ld	a, h
	add	a, b
	ld	b, a
	ld	de, #_wl_tmp
	xor	a, a
	ldh	(#0x1A), a
	ld	a, (de)
	inc	de
	add	a, c
	ld	l, a
	ld	a, b
	adc	a, #0
	ld	h, a
	ld	a, (hl)
	ldh	(#0x30), a
	ld	a, (de)
	inc	de
	add	a, c
	ld	l, a
	ld	a, b
	adc	a, #0
	ld	h, a
	ld	a, (hl)
	ldh	(#0x31), a
	ld	a, (de)
	inc	de
	add	a, c
	ld	l, a
	ld	a, b
	adc	a, #0
	ld	h, a
	ld	a, (hl)
	ldh	(#0x32), a
	ld	a, (de)
	inc	de
	add	a, c
	ld	l, a
	ld	a, b
	adc	a, #0
	ld	h, a
	ld	a, (hl)
	ldh	(#0x33), a
	ld	a, (de)
	inc	de
	add	a, c
	ld	l, a
	ld	a, b
	adc	a, #0
	ld	h, a
	ld	a, (hl)
	ldh	(#0x34), a
	ld	a, (de)
	inc	de
	add	a, c
	ld	l, a
	ld	a, b
	adc	a, #0
	ld	h, a
	ld	a, (hl)
	ldh	(#0x35), a
	ld	a, (de)
	inc	de
	add	a, c
	ld	l, a
	ld	a, b
	adc	a, #0
	ld	h, a
	ld	a, (hl)
	ldh	(#0x36), a
	ld	a, (de)
	inc	de
	add	a, c
	ld	l, a
	ld	a, b
	adc	a, #0
	ld	h, a
	ld	a, (hl)
	ldh	(#0x37), a
	ld	a, (de)
	inc	de
	add	a, c
	ld	l, a
	ld	a, b
	adc	a, #0
	ld	h, a
	ld	a, (hl)
	ldh	(#0x38), a
	ld	a, (de)
	inc	de
	add	a, c
	ld	l, a
	ld	a, b
	adc	a, #0
	ld	h, a
	ld	a, (hl)
	ldh	(#0x39), a
	ld	a, (de)
	inc	de
	add	a, c
	ld	l, a
	ld	a, b
	adc	a, #0
	ld	h, a
	ld	a, (hl)
	ldh	(#0x3A), a
	ld	a, (de)
	inc	de
	add	a, c
	ld	l, a
	ld	a, b
	adc	a, #0
	ld	h, a
	ld	a, (hl)
	ldh	(#0x3B), a
	ld	a, (de)
	inc	de
	add	a, c
	ld	l, a
	ld	a, b
	adc	a, #0
	ld	h, a
	ld	a, (hl)
	ldh	(#0x3C), a
	ld	a, (de)
	inc	de
	add	a, c
	ld	l, a
	ld	a, b
	adc	a, #0
	ld	h, a
	ld	a, (hl)
	ldh	(#0x3D), a
	ld	a, (de)
	inc	de
	add	a, c
	ld	l, a
	ld	a, b
	adc	a, #0
	ld	h, a
	ld	a, (hl)
	ldh	(#0x3E), a
	ld	a, (de)
	inc	de
	add	a, c
	ld	l, a
	ld	a, b
	adc	a, #0
	ld	h, a
	ld	a, (hl)
	ldh	(#0x3F), a
	ld	a, #0x80
	ldh	(#0x1A), a
	ld	a, #0x20
	ldh	(#0x1C), a
	ld	a, (#_wl_hi)
	or	a, #0x80
	ldh	(#0x1E), a
	pop	af
	ldh	(__current_bank), a
	ld	(#0x2000), a
	ret

;  uint16_t mul8x8(uint8_t a, uint8_t b)  -- unsigned 8x8 -> 16 (sdcccall 1:
;  a in A, b in E, result in BC).  ~250 cycles, half of SDCC's __mulint.
_mul8x8::
	ld	hl, #0
	ld	c, a
	ld	b, #0
	ld	d, #8
1$:	srl	e
	jr	nc, 2$
	add	hl, bc
2$:	sla	c
	rl	b
	dec	d
	jr	nz, 1$
	ld	c, l
	ld	b, h
	ret
