/* this is not complete code, this is a fragment included by the various "cores" provided.
 * context: inside a decode function.
 * 
 * void minx86dec_decodeall_x64(struct minx86dec_state_x64 *state,struct minx86dec_instruction_x64 *ins) {
 * minx86_read_ptr_t *cip = state->read_ip;
 * #define core_level 0|1|2|3|4|5|6
 * #define isdata64 0 or state->data64
 * #define isdata32 0 or state->data32
 * #define isaddr32 0 or state->addr32
 *
 * This code decompiles x86-64 64-bit instructions. Be warned that as a hack/kludge of the existing
 * 32-bit instruction set, it's a goddamn mess to parse and handle these opcodes. But hey, it's
 * possible!
 *
 * This code assumes that the native "optimal" datatype is at least 64 bits, since everything is
 * either 64-bits or 32-bit sign-extended to 64-bit. Obviously, if you're trying to compile this
 * for DOS and the compiler doesn't support 64-bit integers, you're out of luck. sorry.
 *
 * WARNING: this code is very incomplete at this time!
 *
 * ready? here we go! */
#define data32wordsize (isdata32 ? 4 : 2)
#define addr32wordsize (isaddr32 ? 4 : 2)
#define data64wordsize (isdata64 ? 8 : data32wordsize)
#define seg_can_override(x) (ins->segment >= 0 ? ins->segment : (x))

#define COVER_4(x) case (x): case (x+1): case (x+2): case (x+3)
#define COVER_2(x) case (x): case (x+1)

#define COVER_8(x) COVER_4(x): COVER_4(x+4)
#define COVER_ROW(x) COVER_8(x): COVER_8(x+8)

/* defaults */
#ifndef core_level
#  define core_level 0
/* 0 = 8086
 * 1 = 80186
 * 2 = 286
 * 3 = 386
 * 4 = 486
 * 5 = pentium or higher
 * 6 = pentium II or higher */
#endif

#ifndef fpu_level
#  define fpu_level -1
/* 0 = 8087
 * 2 = 287
 * 3 = 387
 * 4 = 487
 * 5 = pentium or higher
 * 6 = pentium II or higher */
#endif

#ifndef amd_3dnow
#  define amd_3dnow -1
/* 1 = 3dnow!
 * 2 = 3dnow2! */
#endif

#ifndef sse_level
#  define sse_level -1
/* 1 = SSE
 * 2 = SSE2
 * 3 = SSE3
 * 4 = SSE4 */
#endif

#ifndef pentium
#  define pentium 0
/* 1 = pentium/ppro
 * 2 = pentium II
 * 3 = pentium III
 * 4 = pentium 4 */
#endif

#ifdef pentiumpro
/* pentium == 1 -> pentium pro */
#endif

/* did we encounter FWAIT? (another odd prefix tacked on by Intel to instructions, yech!!) */
int fwait = 0;

ins->lock = 0;
ins->rex.raw = 0;
ins->rep = MX86_REP_NONE;
decode_next:
{
	const uint8_t first_byte = *cip++;
	switch (first_byte) {
		/* group 0x00-0x3F opcodes */
		COVER_2(0x04):	/* ADD */	COVER_2(0x0C):	/* OR */
		COVER_2(0x14):	/* ADC */	COVER_2(0x1C):	/* SBB */
		COVER_2(0x24):	/* AND */	COVER_2(0x2C):	/* SUB */
		COVER_2(0x34):	/* XOR */	COVER_2(0x3C):	/* CMP */
			ins->opcode = MXOP_ADD+(first_byte>>3);
			ins->argc = 2; {
				struct minx86dec_argv_x64 *imm = &ins->argv[1];
				struct minx86dec_argv_x64 *reg = &ins->argv[0];
				imm->size = reg->size = (first_byte & 1) ? data64wordsize : 1;
				set_immediate(imm,(first_byte & 1) ? imm32sbysize(ins) : fetch_u8());
				set_register(reg,MX86_REG_AX);
			} break;
		/* group 0x00-0x3F opcodes */
		COVER_4(0x00):	/* ADD */	COVER_4(0x08):	/* OR */
		COVER_4(0x10):	/* ADC */	COVER_4(0x18):	/* SBB */
		COVER_4(0x20):	/* AND */	COVER_4(0x28):	/* SUB */
		COVER_4(0x30):	/* XOR */	COVER_4(0x38):	/* CMP */
			ins->opcode = MXOP_ADD+(first_byte>>3);
			ins->argc = 2; {
				const int which = (first_byte>>1)&1;
				struct minx86dec_argv_x64 *rm = &ins->argv[which];
				struct minx86dec_argv_x64 *reg = &ins->argv[which^1];
				struct x64_mrm mrm = decode_rm_x64(rm,ins,reg->size,PLUSR_TRANSFORM);
				rm->size = reg->size = (first_byte & 1) ? data64wordsize : 1;
				rm->segment = seg_can_override(MX86_SEG_DS);
				set_register(reg,plusr_transform(ins->rex,reg->size,mrm.f.reg));
			} break;
		/* REX prefix */
		COVER_ROW(0x40):
			ins->data64 = (first_byte >> 3) & 1;
			ins->rex.raw = first_byte;
			if (--patience) goto decode_next;
			goto decode_next;
		/* 386+ instruction 32-bit prefixes */
		case 0x66: /* 32-bit data override */
			ins->data32 ^= 1;
			dataprefix32++;
			if (--patience) goto decode_next;
			break;

		case 0x67: /* 32-bit address override */
			ins->addr32 ^= 1;
			addrprefix32++;
			if (--patience) goto decode_next;
			break;

		case 0x8D: /* LEA reg,mem */
			ins->opcode = MXOP_LEA;
			ins->argc = 2; {
				struct minx86dec_argv_x64 *reg = &ins->argv[0];
				struct minx86dec_argv_x64 *rm = &ins->argv[1];
				struct x64_mrm mrm = decode_rm_x64(rm,ins,reg->size,PLUSR_TRANSFORM);
				rm->segment = seg_can_override(MX86_SEG_DS);
				rm->size = reg->size = data64wordsize;
				set_register(reg,mrm.f.reg);
			} break;

		/* NOP */
		case 0x90:
			if (ins->rep == MX86_REPNE) ins->opcode = MXOP_PAUSE;
			else ins->opcode = MXOP_NOP;
			break;

		/* MOV a,imm */
		COVER_ROW(0xB0):
			ins->opcode = MXOP_MOV;
			ins->argc = 2; {
				struct minx86dec_argv_x64 *r = &ins->argv[0];
				struct minx86dec_argv_x64 *imm = &ins->argv[1];
				r->size = imm->size = (first_byte & 8) ? data64wordsize : 1;
				set_immediate(imm,(first_byte & 8) ? imm64bysize(ins) : fetch_u8());
				set_register(r,first_byte & 7);
			} break;

		default:
			/* fall through */
			break;
	};
}

#undef data32wordsize

/* } */
