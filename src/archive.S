	.assume	adl=1

	.equ	ti.flags, 0xD00080
	.equ	ti.Mov9ToOP1, 0x020320
	.equ	ti.ChkFindSym, 0x02050C
	.equ	ti.ChkInRam, 0x021F98
	.equ	ti.Arc_Unarc, 0x021448

	.section	.text._archive_var
	.global	_archive_var
	.type	_archive_var, @function
_archive_var:
	pop	de
	ex	(sp), hl
	push	de
	push	iy
	ld	iy, ti.flags
	call	ti.Mov9ToOP1
	call	ti.ChkFindSym
	jr	c, .L.done
	call	ti.ChkInRam
	call	z, ti.Arc_Unarc
.L.done:
	pop	iy
	ret
