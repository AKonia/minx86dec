
#if defined(ENABLE_64BIT)
static inline uint64_t fetch_u64() {
	const register uint64_t r = *((uint64_t*)(cip));
	cip += 8;
	return r;
}
static inline int64_t fetch_s64() { return (int64_t)(fetch_u64()); }
#endif

static inline uint32_t fetch_u32() {
	const register uint32_t r = *((uint32_t*)(cip));
	cip += 4;
	return r;
}
static inline int32_t fetch_s32() { return (int32_t)(fetch_u32()); }

static inline uint16_t fetch_u16() {
	const register uint16_t r = *((uint16_t*)(cip));
	cip += 2;
	return r;
}
static inline int16_t fetch_s16() { return (int16_t)(fetch_u16()); }

static inline uint8_t fetch_u8() {
	return *cip++;
}
static inline int8_t fetch_s8() { return (int8_t)(fetch_u8()); }

union x64_mrm_byte {
	struct {
		uint8_t		rm:3;
		uint8_t		reg:3;
		uint8_t		mod:2;
	} f;
	uint8_t		raw;
};

union x64_sib_byte {
	struct {
		uint8_t		base:3;
		uint8_t		index:3;
		uint8_t		scale:2;
	} f;
	uint8_t		raw;
};

/* combined MRM + SIB + REX */
struct x64_mrm {
	struct {
		/* mrm */
		uint8_t		rm:4;
		uint8_t		reg:4;
		uint8_t		mod:2;
		/* sib */
		uint8_t		scale:2;
		uint8_t		index:4;
		uint8_t		base:4;
	} f;
	uint32_t	raw;
};

static int rm_addr16_mapping[8] = {
	MX86_REG_BX,	MX86_REG_BX,	MX86_REG_BP,	MX86_REG_BP,
	MX86_REG_SI,	MX86_REG_DI,	MX86_REG_BP,	MX86_REG_BX	};

/* read immediate, 64-bit if 64-bit enabled */
static inline uint64_t imm64bysize(struct minx86dec_instruction_x64 *s) {
	if (s->data64) return fetch_u64();
	if (s->data32) return (uint64_t)fetch_u32();
	return (uint64_t)fetch_u16();
}

/* read immediate, 32-bit sign-extended if 64-bit enabled */
static inline uint64_t imm32sbysize(struct minx86dec_instruction_x64 *s) {
	if (s->data64) return (uint64_t)((int32_t)fetch_u32());
	if (s->data32) return (uint64_t)fetch_u32();
	return (uint64_t)fetch_u16();
}

/* read immediate, 32-bit zero-extended if 64-bit enabled */
static inline uint64_t imm32zbysize(struct minx86dec_instruction_x64 *s) {
	if (s->data64) return (uint64_t)fetch_u32();
	if (s->data32) return (uint64_t)fetch_u32();
	return (uint64_t)fetch_u16();
}

static inline uint64_t data32_64_signextend(int is64,uint32_t x) {
	if (is64) return (uint64_t)((int32_t)(x));
	return (uint64_t)x;
}

static inline uint64_t data32_64_zeroextend(int is64,uint32_t x) {
	if (is64) return (uint64_t)(x);
	return (uint64_t)x;
}

static inline void set_mem_ref_reg(struct minx86dec_argv_x64 *a,int reg_base) {
	a->regtype = MX86_RT_NONE;
	a->scalar = 0;
	a->memregs = 1;
	a->memref_base = 0;
	a->memreg[0] = reg_base;
}

static inline void set_mem_ref_imm(struct minx86dec_argv_x64 *a,uint64_t base) {
	a->regtype = MX86_RT_NONE;
	a->scalar = 0;
	a->memregs = 0;
	a->memref_base = base;
}

static inline void set_fpu_register(struct minx86dec_argv_x64 *a,int reg) {
	a->regtype = MX86_RT_ST;
	a->reg = reg;
}

static inline void set_segment_register(struct minx86dec_argv_x64 *a,int reg) {
	a->regtype = MX86_RT_SREG;
	a->reg = reg;
}

static inline void set_mmx_register(struct minx86dec_argv_x64 *d,int reg) {
	d->regtype = MX86_RT_MMX;
	d->reg = reg;
}

static inline void set_sse_register(struct minx86dec_argv_x64 *d,int reg) {
	d->regtype = MX86_RT_SSE;
	d->reg = reg;
}

static inline void set_register(struct minx86dec_argv_x64 *a,int reg) {
	a->regtype = MX86_RT_REG;
	a->reg = reg;
}

static inline void set_immediate(struct minx86dec_argv_x64 *a,uint64_t reg) {
	a->regtype = MX86_RT_IMM;
	a->value = reg;
}

static inline void set_control_register(struct minx86dec_argv_x64 *a,uint32_t reg) {
	a->regtype = MX86_RT_CR;
	a->reg = reg;
}

/* given a reg value, transform it for those +rb/+rw/etc instruction encodings */
static inline uint32_t plusr_transform(union minx86dec_rex_x64 rex,uint32_t size,uint32_t reg) {
	if (rex.f.prefix && size == 1 && ((reg&(~3)) == 4)) return (reg - 4) + MX86_REG_SPL;
	return reg;
}

#define PLUSR_TRANSFORM 1

static inline struct x64_mrm decode_rm_x64(struct minx86dec_argv_x64 *a,struct minx86dec_instruction_x64 *s,uint32_t size,int transform) {
	union x64_mrm_byte r_mrm; r_mrm.raw = fetch_u8();
	union minx86dec_rex_x64 rex = s->rex;
	struct x64_mrm mrm;

	/* now we need to assemble the actual values from the REX bytes */
	mrm.raw = 0;
	mrm.f.rm = r_mrm.f.rm | (rex.f.b << 3);
	mrm.f.reg = r_mrm.f.reg | (rex.f.r << 3);
	mrm.f.mod = r_mrm.f.mod;

	if (mrm.f.mod == 3) {
		a->regtype = MX86_RT_REG;
		if (transform) a->reg = plusr_transform(rex,size,mrm.f.rm);
		else a->reg = mrm.f.rm;
		return mrm;
	}
	a->regtype = MX86_RT_NONE;

	a->scalar = 0;
	a->memregsz = s->addr32 ? 4 : 8;
	a->memref_base = 0;

	if (mrm.f.mod == 0) {
		if (mrm.f.rm == 5) {
			if (!s->addr32) { /* RIP relative */
				a->memreg[0] = MX86_REG_RIP;
				a->memregs = 1;
			}
			else {
				a->memregs = 0;
			}
			a->memref_base = (uint64_t)((int32_t)fetch_u32()); /* sign-extended */
			return mrm;
		}
	}

	if ((mrm.f.rm&7) == 4) { /* SIB byte follows */
		union x64_sib_byte sib;
		mrm.f.rm &= 7;
		sib.raw = fetch_u8();
		mrm.f.scale = sib.f.scale;
		mrm.f.index = sib.f.index | (rex.f.x << 3);
		mrm.f.base = sib.f.base | (rex.f.b << 3);

		if (sib.f.index == 4) {
			if (sib.f.base != 5) {
				a->memregs = 1;
				a->memreg[0] = sib.f.base;
			}
			else {
				a->memregs = 0;
			}
		}
		else {
			if (sib.f.base != 5) {
				a->memregs = 2;
				a->scalar = sib.f.scale;
				a->memreg[0] = sib.f.index;
				a->memreg[1] = sib.f.base;
			}
			else {
				a->memregs = 1;
				a->scalar = sib.f.scale;
				a->memreg[0] = sib.f.index;
			}
		}

		if (sib.f.base == 5 && mrm.f.mod == 0)
			a->memref_base = (uint64_t)((int32_t)fetch_u32());
	}
	else {
		a->memregs = 1;
		a->memreg[0] = mrm.f.rm;
	}

	if (mrm.f.mod == 2)
		a->memref_base = (uint64_t)((int32_t)fetch_u32());
	else if (mrm.f.mod == 1)
		a->memref_base = (uint64_t)((char)fetch_u8());

	return mrm;
}

