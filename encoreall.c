#include "minx86dec/types.h"
#include "minx86dec/state.h"
#include "minx86dec/opcodes.h"
#include "minx86dec/coreall.h"
#include "minx86dec/opcodes_str.h"
#include "minx86dec/encoreall.h"
#include <string.h>
#include <stdio.h>

/* code will call this to generate the appropriate 386 data override prefix.
 * if 16-bit mode and the arg provided is 32 bits wide, OR
 * if 32-bit mode and the arg provided is 16 bits wide */
static inline minx86_write_ptr_t minx86enc_32_overrides(struct minx86dec_argv *a,struct minx86enc_state *est,minx86_write_ptr_t o) {
	if (a->memregs != 0 && ((a->memregsz>>2)^(est->addr32))) *o++ = 0x67;
	if ((a->size>>2)^(est->data32)) *o++ = 0x66;
	return o;
}

static inline minx86_write_ptr_t minx86enc_32_overrides_far(struct minx86dec_argv *a,struct minx86enc_state *est,minx86_write_ptr_t o) {
	if (a->memregs != 0 && ((a->memregsz>>2)^(est->addr32))) *o++ = 0x67;
	if (((a->size==6)?1:0)^(est->data32)) *o++ = 0x66;
	return o;
}

minx86_write_ptr_t minx86enc_encode_memreg_far(struct minx86dec_argv *a,minx86_write_ptr_t o,unsigned int regval) {
	int mod = 0,sib = -1;

	if (a->memref_base != 0) {
		if (	(a->memregsz == 4 && (int32_t)a->memref_base >= -0x80 && (int32_t)a->memref_base < 0x80) ||
			(a->memregsz == 2 && (int16_t)a->memref_base >= -0x80 && (int16_t)a->memref_base < 0x80))
			mod = 1;
		else
			mod = 2;
	}

	if (a->memregs == 1) {
		if (a->memregsz == 4) {/* 32-bit */
			if (mod == 0) {
				if (a->memreg[0] == 5)
					mod = 1;	/* [EBP] -> [EBP+0] */
			}
			*o++ = (mod << 6) | (regval << 3) | a->memreg[0];
			{
				if (a->memreg[0] == 4) {
					/* the code for [ESP] is used instead for
					 * encoding the SIB so we must encode the SIB byte
					 * to properly make [ESP] reference */
					*o++ = (a->scalar << 6) | (4 << 3) | MX86_REG_ESP; /* scale=s/index=none/base=ESP */
				}
			}
		}
		else {/* 16-bit */
			switch (a->memreg[0]) {
				case MX86_REG_SI:	*o++ = 4 | (regval << 3) | (mod<<6); break;
				case MX86_REG_DI:	*o++ = 5 | (regval << 3) | (mod<<6); break;
				case MX86_REG_BP: {
					if (mod == 0) mod = 1;	/* No such [BP] must encode as [BP+0] */
					*o++ = 6 | (regval << 3) | (mod<<6); } break;
				case MX86_REG_BX:	*o++ = 7 | (regval << 3) | (mod<<6); break;
			}
		}
	}
	else if (a->memregs == 2) {
		if (a->memregsz == 4) {/* 32-bit */
			if (mod == 0) {
				if (a->memreg[0] == 5 || a->memreg[1] == 5)
					mod = 1;	/* [EBP] -> [EBP+0] */
			}
			*o++ = (mod << 6) | (regval << 3) | 4;	/* SIB */
			*o++ = (a->scalar << 6) | (a->memreg[0] << 3) | a->memreg[1]; /* scale=s/index=none/base=ESP */
		}
		else {/* 16-bit */
			uint8_t c = 0;

			/* BX or BP */
			switch (a->memreg[0]) {
				case MX86_REG_BP:	c = 2; break;
			}
			/* second one may be SI or DI */
			switch (a->memreg[1]) {
				case MX86_REG_DI:	c |= 1; break;
			}

			*o++ = c | (regval << 3) | (mod<<6);
		}
	}

	if (mod == 1)
		*o++ = (uint8_t)(a->memref_base);
	else if (mod == 2) {
		if (a->memregsz == 4) {
			*((uint32_t*)o) = (uint32_t)(a->memref_base);
			o += 4;
		}
		else {
			*((uint16_t*)o) = (uint16_t)(a->memref_base);
			o += 2;
		}
	}

	return o;
}

minx86_write_ptr_t minx86enc_encode_memreg(struct minx86dec_argv *a,minx86_write_ptr_t o,unsigned int regval) {
	int mod = 0,sib = -1;

	if (a->memref_base != 0) {
		if (	(a->memregsz == 4 && (int32_t)a->memref_base >= -0x80 && (int32_t)a->memref_base < 0x80) ||
			(a->memregsz == 2 && (int16_t)a->memref_base >= -0x80 && (int16_t)a->memref_base < 0x80))
			mod = 1;
		else
			mod = 2;
	}

	if (a->memregs == 1) {
		if (a->memregsz == 4) {/* 32-bit */
			if (mod == 0) {
				if (a->memreg[0] == 5)
					mod = 1;	/* [EBP] -> [EBP+0] */
			}
			*o++ = (mod << 6) | (regval << 3) | a->memreg[0];
			{
				if (a->memreg[0] == 4) {
					/* the code for [ESP] is used instead for
					 * encoding the SIB so we must encode the SIB byte
					 * to properly make [ESP] reference */
					*o++ = (a->scalar << 6) | (4 << 3) | MX86_REG_ESP; /* scale=s/index=none/base=ESP */
				}
			}
		}
		else {/* 16-bit */
			switch (a->memreg[0]) {
				case MX86_REG_SI:	*o++ = 4 | (regval << 3) | (mod<<6); break;
				case MX86_REG_DI:	*o++ = 5 | (regval << 3) | (mod<<6); break;
				case MX86_REG_BP: {
					if (mod == 0) mod = 1;	/* No such [BP] must encode as [BP+0] */
					*o++ = 6 | (regval << 3) | (mod<<6); } break;
				case MX86_REG_BX:	*o++ = 7 | (regval << 3) | (mod<<6); break;
			}
		}
	}
	else if (a->memregs == 2) {
		if (a->memregsz == 4) {/* 32-bit */
			if (mod == 0) {
				if (a->memreg[1] == 5)
					mod = 1;	/* [EBP] -> [EBP+0] */
			}
			*o++ = (mod << 6) | (regval << 3) | 4;	/* SIB */
			*o++ = (a->scalar << 6) | (a->memreg[0] << 3) | a->memreg[1]; /* scale=s/index=none/base=ESP */
		}
		else {/* 16-bit */
			uint8_t c = 0;

			/* BX or BP */
			switch (a->memreg[0]) {
				case MX86_REG_BP:	c = 2; break;
			}
			/* second one may be SI or DI */
			switch (a->memreg[1]) {
				case MX86_REG_DI:	c |= 1; break;
			}

			*o++ = c | (regval << 3) | (mod<<6);
		}
	}

	if (mod == 1)
		*o++ = (uint8_t)(a->memref_base);
	else if (mod == 2) {
		if (a->memregsz == 4) {
			*((uint32_t*)o) = (uint32_t)(a->memref_base);
			o += 4;
		}
		else {
			*((uint16_t*)o) = (uint16_t)(a->memref_base);
			o += 2;
		}
	}

	return o;
}

void minx86enc_encodeall(struct minx86enc_state *est,struct minx86dec_instruction *ins) {
	minx86_write_ptr_t o = est->write_ip;
	est->started_here = o;

	switch (ins->opcode) {
		case MXOP_JMP: { /*====================NEAR JMP===================*/
			struct minx86dec_argv *a = &ins->argv[0];
			if (a->regtype == MX86_RT_REG) {
				o = minx86enc_32_overrides(a,est,o);
				*o++ = 0xFF; *o++ = (3<<6) | (4<<3) | a->reg;	/* mod=3 reg=4 rm=reg */
			}
			else if (a->regtype == MX86_RT_NONE) {
				o = minx86enc_32_overrides(a,est,o);
				*o++ = 0xFF; o = minx86enc_encode_memreg(a,o,4);
			}
			else if (a->regtype == MX86_RT_IMM) { /* hope you set est->ip_value! encoding is RELATIVE! */
				int32_t delta = (int32_t)(a->value - est->ip_value),extra = (int32_t)(o - est->started_here);
				/* if it's small enough, encode as single-byte JMP */
				if ((delta-(2+extra)) >= -0x80 && (delta-(2+extra)) < 0x80)
					{ o = minx86enc_32_overrides(a,est,o); *o++ = 0xEB; *o++ = (uint8_t)(delta-(2+extra)); }
				/* if the encoding is for 32-bit mode, OR the delta is too large for 16-bit mode: */
				else if (est->addr32 || !((delta-(5+extra)) >= -0x8000 && (delta-(5+extra)) < 0x8000))
					{ if (!est->addr32) { *o++ = 0x66; extra++; }; *o++ = 0xE9; *((uint32_t*)o) = (uint32_t)(delta-(5+extra)); o += 4; }
				else
					{ *o++ = 0xE9; *((uint16_t*)o) = (uint16_t)(delta-(3+extra)); o += 2; }
			}
		} break;
		case MXOP_JMP_FAR: { /*=====================FAR JMP====================*/
			struct minx86dec_argv *ofs=&ins->argv[0];
			if (ofs->regtype == MX86_RT_IMM) { /* easy: seg:off values are absolute */
				if (est->addr32 || (ofs->value & 0xFFFF0000)) {
					if (!est->addr32) *o++ = 0x66;
					*o++ = 0xEA;
					*((uint32_t*)o) = (uint32_t)ofs->value; o += 4;
				}
				else {
					*o++ = 0xEA;
					*((uint16_t*)o) = (uint16_t)ofs->value; o += 2;
				}

				/* segment */
				*((uint16_t*)o) = (uint16_t)ofs->segval; o += 2;
			}
			else if (ofs->regtype == MX86_RT_NONE) {
				o = minx86enc_32_overrides_far(ofs,est,o);
				*o++ = 0xFF; o = minx86enc_encode_memreg_far(ofs,o,5);
			}
		} break;
		case MXOP_CALL: { /*====================NEAR CALL===================*/
			struct minx86dec_argv *a = &ins->argv[0];
			if (a->regtype == MX86_RT_REG) {
				o = minx86enc_32_overrides(a,est,o);
				*o++ = 0xFF; *o++ = (3<<6) | (2<<3) | a->reg;	/* mod=3 reg=6 rm=reg */
			}
			else if (a->regtype == MX86_RT_NONE) {
				o = minx86enc_32_overrides(a,est,o);
				*o++ = 0xFF; o = minx86enc_encode_memreg(a,o,2);
			}
			else if (a->regtype == MX86_RT_IMM) { /* hope you set est->ip_value! encoding is RELATIVE! */
				int32_t delta = (int32_t)(a->value - est->ip_value),extra = (int32_t)(o - est->started_here);
				/* no such thing as single-byte CALL. */
				/* if the encoding is for 32-bit mode, OR the delta is too large for 16-bit mode: */
				if (est->addr32 || !((delta-(5+extra)) >= -0x8000 && (delta-(5+extra)) < 0x8000))
					{ if (!est->addr32) { *o++ = 0x66; extra++; }; *o++ = 0xE8; *((uint32_t*)o) = (uint32_t)(delta-(5+extra)); o += 4; }
				else
					{ *o++ = 0xE8; *((uint16_t*)o) = (uint16_t)(delta-(3+extra)); o += 2; }
			}
		} break;
		case MXOP_CALL_FAR: { /*=====================FAR CALL====================*/
			struct minx86dec_argv *ofs=&ins->argv[0];
			if (ofs->regtype == MX86_RT_IMM) { /* easy: seg:off values are absolute */
				if (est->addr32 || (ofs->value & 0xFFFF0000)) {
					if (!est->addr32) *o++ = 0x66;
					*o++ = 0x9A;
					*((uint32_t*)o) = (uint32_t)ofs->value; o += 4;
				}
				else {
					*o++ = 0x9A;
					*((uint16_t*)o) = (uint16_t)ofs->value; o += 2;
				}

				/* segment */
				*((uint16_t*)o) = (uint16_t)ofs->segval; o += 2;
			}
			else if (ofs->regtype == MX86_RT_NONE) {
				o = minx86enc_32_overrides_far(ofs,est,o);
				*o++ = 0xFF; o = minx86enc_encode_memreg_far(ofs,o,3);
			}
		} break;
		case MXOP_NOP: /*====================NOP====================*/ *o++ = 0x90; break;
		case MXOP_XCHG: { /*========================XCHG======================*/
			struct minx86dec_argv *a=&ins->argv[0],*b=&ins->argv[1];
			unsigned char word = (a->size >= 2) ? 1 : 0;
			/* make sure a is r/m and b is reg. ASSUME: both are the same datasize */
			if (b->regtype == MX86_RT_NONE) { struct minx86dec_argv *t = a; a = b; b = t; }
			if (word) o = minx86enc_32_overrides(a,est,o);

			if (a->regtype == MX86_RT_REG) {
				if (word && a->reg == MX86_REG_AX)
					*o++ = 0x90 + b->reg;
				else if (word && b->reg == MX86_REG_AX)
					*o++ = 0x90 + a->reg;
				else {
					*o++ = 0x86 + word;
					*o++ = (3 << 6) | (b->reg << 3) | a->reg;
				}
			}
			else { /* xchg r/m, reg */
				*o++ = 0x86 + word; o = minx86enc_encode_memreg_far(a,o,b->reg);
			}
		} break;
		case MXOP_MOV: { /*====================MOV=====================*/
			struct minx86dec_argv *a=&ins->argv[0],*b=&ins->argv[1];
			unsigned char word = (a->size >= 2) ? 1 : 0;
			/* it doesn't matter if it's reg-reg, reg-r/m, r/m-reg, etc
			 * instruction encoding covers them all */
			if (word) o = minx86enc_32_overrides(a,est,o);

			if (a->regtype == MX86_RT_REG && b->regtype == MX86_RT_REG) {
				*o++ = 0x88+word; *o++ = (3<<6) | (b->reg<<3) | (a->reg);
			}
		} break;
	}

	est->write_ip = o;
}

