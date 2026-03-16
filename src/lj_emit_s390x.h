/*
** s390x instruction emitter.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

/* -- Instruction formats ------------------------------------------------- */

typedef uint32_t S390Ins;

/* s390x instruction format encodings (bits 0-15 are the opcode) */

/* RR format: 2 bytes, r1/r2 in byte 1 */
#define S390F_RR        0x0000

/* RRE format: 4 bytes, r1/r2 in byte 3 */
#define S390F_RRE       0x00010000

/* RRF format: 4 bytes, r1/r3/r2 in bytes 2-3 */
#define S390F_RRF       0x00020000

/* RX format: 4 bytes, r1/d2(x2,b2) */
#define S390F_RX        0x00030000

/* RXY format: 6 bytes, r1/d2(x2,b2) with extended displacement */
#define S390F_RXY       0x00040000

/* RS format: 4 bytes, r1/r3/d2(b2) */
#define S390F_RS        0x00050000

/* RSY format: 6 bytes, r1/r3/d2(b2) with extended displacement */
#define S390F_RSY       0x00060000

/* RI format: 4 bytes, r1/i2 */
#define S390F_RI        0x00070000

/* RIL format: 6 bytes, r1/i2 (32-bit immediate) */
#define S390F_RIL       0x00080000

/* SI format: 4 bytes, d1(b1)/i2 */
#define S390F_SI        0x00090000

/* Instruction opcodes (bits 0-15) */
#define S390I_LGR       0xb904  /* Load GR (64-bit) */
#define S390I_LG        0xe3    /* Load (64-bit) - RX format */
#define S390I_LG_RX     0x04    /* LG second byte */
#define S390I_LG_RXY    0xe304  /* Load (64-bit) - RXY */
#define S390I_STG       0xe3    /* Store (64-bit) - RX */
#define S390I_STG_RX    0x24    /* STG second byte */
#define S390I_STG_RXY   0xe324  /* Store (64-bit) - RXY */
#define S390I_AGR       0xb908  /* Add (64-bit) */
#define S390I_SGR       0xb909  /* Subtract (64-bit) */
#define S390I_MGR       0xb90c  /* Multiply (64-bit) */
#define S390I_DGR       0xb90d  /* Divide (64-bit) */
#define S390I_LCGFR     0xb913  /* Load Complement (64-bit) */
#define S390I_NGR       0xb980  /* And (64-bit) */
#define S390I_OGR       0xb981  /* Or (64-bit) */
#define S390I_XGR       0xb982  /* Xor (64-bit) */
#define S390I_CGR       0xb920  /* Compare (64-bit) */
#define S390I_CG        0xe3    /* Compare (64-bit) - RX */
#define S390I_CG_RX     0x20    /* CG second byte */
#define S390I_CG_RXY    0xe320  /* Compare (64-bit) - RXY */
#define S390I_AGHI      0xa7b   /* Add Halfword Immediate (64-bit) */
#define S390I_CGHI      0xa7f   /* Compare Halfword Immediate (64-bit) */
#define S390I_LGHI      0xa709  /* Load Halfword Immediate (64-bit) */
#define S390I_LGFI      0xc001  /* Load Fullword Immediate (64-bit) */
#define S390I_LGRL      0xc408  /* Load Relative Long (64-bit) */
#define S390I_BR        0x07f0  /* Branch Register */
#define S390I_BRC       0xa704  /* Branch Relative on Condition */
#define S390I_BRCL      0xc004  /* Branch Relative on Condition Long */
#define S390I_BASR      0x0d00  /* Branch and Save Register */
#define S390I_BCTR      0x06    /* Branch on Count */
#define S390I_SLLG      0xeb    /* Shift Left Logical (64-bit) */
#define S390I_SLLG_RSY  0xeb0d  /* Shift Left Logical - RSY */
#define S390I_SRLG      0xeb    /* Shift Right Logical (64-bit) */
#define S390I_SRLG_RSY  0xeb0c  /* Shift Right Logical - RSY */
#define S390I_SRAG      0xeb    /* Shift Right Arithmetic (64-bit) */
#define S390I_SRAG_RSY  0xeb0a  /* Shift Right Arithmetic - RSY */
#define S390I_SLGFI     0xc20c  /* Subtract Logical Fullword Immediate */
#define S390I_ALGFI     0xc20a  /* Add Logical Fullword Immediate */
#define S390I_CLGFI     0xc20e  /* Compare Logical Fullword Immediate */
#define S390I_LTGR      0xb902  /* Load and Test (64-bit) */
#define S390I_LA        0x41    /* Load Address */
#define S390I_LAY       0xe371  /* Load Address - Long */
#define S390I_LARL      0xc000  /* Load Address Relative Long */
#define S390I_STMG      0xeb    /* Store Multiple (64-bit) */
#define S390I_STMG_RSY  0xeb24  /* Store Multiple - RSY */
#define S390I_LMG       0xeb    /* Load Multiple (64-bit) */
#define S390I_LMG_RSY   0xeb04  /* Load Multiple - RSY */
#define S390I_IIHF      0xc008  /* Insert Immediate High (high 32 bits) */
#define S390I_IILF      0xc009  /* Insert Immediate Low (low 32 bits) */
#define S390I_NIHF      0xc00a  /* And Immediate High */
#define S390I_NILF      0xc00b  /* And Immediate Low */
#define S390I_OIHF      0xc00c  /* Or Immediate High */
#define S390I_OILF      0xc00d  /* Or Immediate Low */
#define S390I_XIHF      0xc00e  /* Xor Immediate High */
#define S390I_XILF      0xc00f  /* Xor Immediate Low */

/* FP instructions */
#define S390I_LD        0xed    /* Load (long) */
#define S390I_LD_RX     0x68    /* LD second byte */
#define S390I_STD       0xed    /* Store (long) */
#define S390I_STD_RX    0x60    /* STD second byte */
#define S390I_ADR       0xb31a  /* Add (long) */
#define S390I_SDR       0xb31b  /* Subtract (long) */
#define S390I_MDR       0xb31c  /* Multiply (long) */
#define S390I_DDR       0xb31d  /* Divide (long) */
#define S390I_LDR       0xb302  /* Load (long) */
#define S390I_LCDFR     0xb313  /* Load Complement (long) */
#define S390I_LNDR      0xb311  /* Load Negative (long) */
#define S390I_LPDR      0xb310  /* Load Positive (long) */
#define S390I_CDR       0xb319  /* Compare (long) */
#define S390I_LCDR      0xb313  /* Load Complement (long) */

/* -- Emit basic instructions --------------------------------------------- */

/* Emit 16-bit value */
static void emit_i16(ASMState *as, S390Ins si)
{
  *--as->mcp = (MCode)(si >> 8);
  *--as->mcp = (MCode)(si & 0xff);
}

/* Emit 32-bit value */
static void emit_i32(ASMState *as, S390Ins si)
{
  *(uint32_t *)(as->mcp-4) = si;
  as->mcp -= 4;
}

/* Emit 48-bit value (RIL format) */
static void emit_i48(ASMState *as, S390Ins sih, S390Ins sil)
{
  emit_i16(as, sih);
  emit_i32(as, sil);
}

/* RR format: opcode(16) r1r2(8) */
static void emit_rr(ASMState *as, S390Ins si, Reg r1, Reg r2)
{
  MCode *p = as->mcp;
  p[-1] = (MCode)((r1 << 4) | (r2 & 0xf));
  p[-2] = (MCode)(si & 0xff);
  p[-3] = (MCode)(si >> 8);
  as->mcp = p - 2;
}

/* RRE format: opcode(16) 0000(8) r1r2(8) */
static void emit_rre(ASMState *as, S390Ins si, Reg r1, Reg r2)
{
  MCode *p = as->mcp;
  p[-1] = (MCode)((r1 << 4) | (r2 & 0xf));
  p[-2] = 0x00;
  p[-3] = (MCode)(si & 0xff);
  p[-4] = (MCode)(si >> 8);
  as->mcp = p - 4;
}

/* RRF format: opcode(16) r3(4)000 r1r2(8) */
static void emit_rrf(ASMState *as, S390Ins si, Reg r1, Reg r3, Reg r2)
{
  MCode *p = as->mcp;
  p[-1] = (MCode)((r1 << 4) | (r2 & 0xf));
  p[-2] = (MCode)(r3 << 4);
  p[-3] = (MCode)(si & 0xff);
  p[-4] = (MCode)(si >> 8);
  as->mcp = p - 4;
}

/* RX format: opcode(8) r1x2(8) b2d2(16) */
static void emit_rx(ASMState *as, S390Ins si, Reg r1, Reg x2, Reg b2, int32_t d2)
{
  MCode *p = as->mcp;
  p[-1] = (MCode)(d2 & 0xff);
  p[-2] = (MCode)(((d2 >> 8) & 0x0f) | ((b2 & 0xf) << 4));
  p[-3] = (MCode)((r1 << 4) | (x2 & 0xf));
  p[-4] = (MCode)(si & 0xff);
  as->mcp = p - 4;
}

/* RXY format: opcode(16) r1x2(8) b2dl(12) dh(4) */
static void emit_rxy(ASMState *as, S390Ins si, Reg r1, Reg x2, Reg b2, int32_t d2)
{
  MCode *p = as->mcp;
  p[-1] = (MCode)(si & 0xff);
  p[-2] = (MCode)(((d2 >> 12) & 0x0f) | ((r1 & 0xf) << 4));
  p[-3] = (MCode)(((d2 >> 8) & 0x0f) | ((b2 & 0xf) << 4));
  p[-4] = (MCode)((x2 << 4) | ((d2 >> 16) & 0x0f));
  p[-5] = (MCode)(((d2 >> 4) & 0xf0) | (r1 & 0xf));
  p[-6] = (MCode)(si >> 8);
  as->mcp = p - 6;
}

/* RS format: opcode(8) r1r3(8) b2d2(16) */
static void emit_rs(ASMState *as, S390Ins si, Reg r1, Reg r3, Reg b2, int32_t d2)
{
  MCode *p = as->mcp;
  p[-1] = (MCode)(d2 & 0xff);
  p[-2] = (MCode)(((d2 >> 8) & 0x0f) | ((b2 & 0xf) << 4));
  p[-3] = (MCode)((r1 << 4) | (r3 & 0xf));
  p[-4] = (MCode)(si & 0xff);
  as->mcp = p - 4;
}

/* RSY format: opcode(16) r1r3(8) b2dl(12) dh(4) */
static void emit_rsy(ASMState *as, S390Ins si, Reg r1, Reg r3, Reg b2, int32_t d2)
{
  MCode *p = as->mcp;
  p[-1] = (MCode)(si & 0xff);
  p[-2] = (MCode)(((d2 >> 12) & 0x0f) | ((r1 & 0xf) << 4));
  p[-3] = (MCode)(((d2 >> 8) & 0x0f) | ((b2 & 0xf) << 4));
  p[-4] = (MCode)((r3 << 4) | ((d2 >> 16) & 0x0f));
  p[-5] = (MCode)(((d2 >> 4) & 0xf0) | (r1 & 0xf));
  p[-6] = (MCode)(si >> 8);
  as->mcp = p - 6;
}

/* RI format: opcode(8) r1op(4) i2(16) */
static void emit_ri(ASMState *as, S390Ins si, Reg r1, int32_t i2)
{
  MCode *p = as->mcp;
  p[-1] = (MCode)(i2 & 0xff);
  p[-2] = (MCode)(i2 >> 8);
  p[-3] = (MCode)((r1 << 4) | ((si >> 4) & 0x0f));
  p[-4] = (MCode)(si & 0xf0);
  as->mcp = p - 4;
}

/* RIL format: opcode(8) r1op(4) i2(32) */
static void emit_ril(ASMState *as, S390Ins si, Reg r1, int32_t i2)
{
  MCode *p = as->mcp;
  *(int32_t *)(p-4) = i2;
  p[-5] = (MCode)((r1 << 4) | ((si >> 4) & 0x0f));
  p[-6] = (MCode)(si & 0xf0);
  as->mcp = p - 6;
}

/* SI format: opcode(8) i2(8) b1d1(16) */
static void emit_si(ASMState *as, S390Ins si, Reg b1, int32_t d1, int32_t i2)
{
  MCode *p = as->mcp;
  p[-1] = (MCode)(d1 & 0xff);
  p[-2] = (MCode)(((d1 >> 8) & 0x0f) | ((b1 & 0xf) << 4));
  p[-3] = (MCode)(i2 & 0xff);
  p[-4] = (MCode)(si & 0xff);
  as->mcp = p - 4;
}

/* -- Emit loads/stores --------------------------------------------------- */

/* Load 64-bit register from memory: LG r, d(b) */
static void emit_loadreg(ASMState *as, Reg r, Reg base, int32_t ofs)
{
  if (ofs >= -524288 && ofs <= 524287) {
    /* Use RXY format for larger offsets */
    emit_rxy(as, S390I_LG_RXY, r, 0, base, ofs);
  } else {
    /* Load offset into scratch register first */
    emit_ril(as, S390I_LGFI, RID_TMP, ofs);
    emit_rre(as, S390I_AGR, RID_TMP, base);
    emit_rxy(as, S390I_LG_RXY, r, 0, RID_TMP, 0);
  }
}

/* Store 64-bit register to memory: STG r, d(b) */
static void emit_storereg(ASMState *as, Reg r, Reg base, int32_t ofs)
{
  if (ofs >= -524288 && ofs <= 524287) {
    /* Use RXY format for larger offsets */
    emit_rxy(as, S390I_STG_RXY, r, 0, base, ofs);
  } else {
    /* Load offset into scratch register first */
    emit_ril(as, S390I_LGFI, RID_TMP, ofs);
    emit_rre(as, S390I_AGR, RID_TMP, base);
    emit_rxy(as, S390I_STG_RXY, r, 0, RID_TMP, 0);
  }
}

/* Load 64-bit immediate into register */
static void emit_loadi64(ASMState *as, Reg r, int64_t i)
{
  if (i == (int16_t)i) {
    /* Can use LGHI for 16-bit signed immediate */
    emit_ri(as, S390I_LGHI, r, (int16_t)i);
  } else if (i == (int32_t)i) {
    /* Use LGFI for 32-bit signed immediate */
    emit_ril(as, S390I_LGFI, r, (int32_t)i);
  } else {
    /* Need to construct 64-bit value in pieces */
    uint32_t hi = (uint32_t)(i >> 32);
    uint32_t lo = (uint32_t)i;
    if (hi == 0) {
      /* Upper 32 bits are zero */
      emit_ril(as, S390I_LGFI, r, (int32_t)lo);
    } else if (lo == 0) {
      /* Lower 32 bits are zero */
      emit_ril(as, S390I_LGFI, r, (int32_t)hi);
      emit_rre(as, S390I_SLLG, r, r, 32);
    } else {
      /* Both halves non-zero */
      emit_ril(as, S390I_LGFI, r, (int32_t)hi);
      emit_ril(as, S390I_SLLG, r, r, 32);
      if ((int32_t)lo < 0) {
        emit_ril(as, S390I_OILF, r, lo);
      } else {
        emit_rre(as, S390I_AGR, r, r);  /* Add lower part */
        emit_ril(as, S390I_LGFI, RID_TMP, (int32_t)lo);
        emit_rre(as, S390I_AGR, r, RID_TMP);
      }
    }
  }
}

/* Load address into register */
#define emit_loada(as, r, addr) \
  emit_loadi64(as, r, (int64_t)(uintptr_t)(addr))

/* Get/set global_State fields */
#define emit_getgl(as, r, field) \
  emit_loadreg(as, r, RID_GL, (int32_t)offsetof(global_State, field))

#define emit_setgl(as, r, field) \
  emit_storereg(as, r, RID_GL, (int32_t)offsetof(global_State, field))

/* Set VM state */
#define emit_setvmstate(as, i) \
  emit_setgl(as, (i), vmstate)

/* -- Emit control-flow instructions -------------------------------------- */

/* Label for short jumps */
typedef MCode *MCLabel;

/* Return label pointing to current PC */
#define emit_label(as)  ((as)->mcp)

/* Compute relative offset for branch */
static LJ_AINLINE int32_t s390_jmprel(MCode *p, MCode *target)
{
  ptrdiff_t delta = target - p;
  lj_assertJ(delta == (int32_t)delta, "branch target out of range");
  return (int32_t)delta;
}

/* Branch relative on condition (short) */
static void emit_branch(ASMState *as, int cc, MCode *target)
{
  MCode *p = as->mcp;
  int32_t delta = s390_jmprel(p, target);
  /* BRC uses 16-bit signed displacement in halfwords */
  if (delta >= -32768 && delta <= 32767) {
    p[-1] = (MCode)(delta & 0xff);
    p[-2] = (MCode)(delta >> 8);
    p[-3] = (MCode)(cc << 4);
    p[-4] = 0xa7;
    as->mcp = p - 4;
  } else {
    /* Use BRCL for longer jumps */
    *(int32_t *)(p-4) = delta;
    p[-5] = (MCode)(cc << 4);
    p[-6] = 0xc0;
    as->mcp = p - 6;
  }
}

/* Conditional branch with condition code */
static void emit_condbranch(ASMState *as, int cc, MCode *target)
{
  emit_branch(as, cc, target);
}

/* Unconditional jump */
static void emit_jmp(ASMState *as, MCode *target)
{
  /* Unconditional branch uses cc=15 */
  emit_branch(as, 15, target);
}

/* Call target */
static void emit_call(ASMState *as, void *target)
{
  MCode *p = as->mcp;
  int32_t delta = s390_jmprel(p, (MCode *)target);
  
  /* Try to use BRASL for relative calls */
  if (delta >= -33554432 && delta <= 33554431) {
    /* BRCL with r14 as link register */
    *(int32_t *)(p-4) = delta;
    p[-5] = 0x05;  /* r0 = 0, but we use r14 */
    p[-6] = 0xc0;
    as->mcp = p - 6;
  } else {
    /* Load target address into r1, then BASR r14, r1 */
    emit_loadi64(as, RID_TMP, (int64_t)(uintptr_t)target);
    emit_rr(as, S390I_BASR, RID_R14, RID_TMP);
  }
}

/* -- Emit generic operations --------------------------------------------- */

/* Generic move between two regs */
static void emit_movrr(ASMState *as, IRIns *ir, Reg dst, Reg src)
{
  UNUSED(ir);
  if (dst != src) {
    if (dst < RID_MAX_GPR && src < RID_MAX_GPR) {
      /* GPR to GPR */
      emit_rre(as, S390I_LGR, dst, src);
    } else if (dst >= RID_MAX_GPR && src >= RID_MAX_GPR) {
      /* FPR to FPR */
      emit_rre(as, S390I_LDR, dst - RID_MIN_FPR, src - RID_MIN_FPR);
    } else {
      /* Cross move - need to go through memory or special handling */
      /* For now, use scratch memory */
      emit_storereg(as, src, RID_SP, SPOFS_TMP);
      emit_loadreg(as, dst, RID_SP, SPOFS_TMP);
    }
  }
}

/* Generic load of register with base and offset */
static void emit_loadofs(ASMState *as, IRIns *ir, Reg r, Reg base, int32_t ofs)
{
  if (r < RID_MAX_GPR) {
    /* GPR load */
    emit_loadreg(as, r, base, ofs);
  } else {
    /* FPR load */
    if (ofs >= -524288 && ofs <= 524287) {
      emit_rxy(as, 0xed68, r - RID_MIN_FPR, 0, base, ofs);  /* LDY */
    } else {
      emit_loadi64(as, RID_TMP, ofs);
      emit_rre(as, S390I_AGR, RID_TMP, base);
      emit_rxy(as, 0xed68, r - RID_MIN_FPR, 0, RID_TMP, 0);
    }
  }
}

/* Generic store of register with base and offset */
static void emit_storeofs(ASMState *as, IRIns *ir, Reg r, Reg base, int32_t ofs)
{
  if (r < RID_MAX_GPR) {
    /* GPR store */
    emit_storereg(as, r, base, ofs);
  } else {
    /* FPR store */
    if (ofs >= -524288 && ofs <= 524287) {
      emit_rxy(as, 0xed60, r - RID_MIN_FPR, 0, base, ofs);  /* STDY */
    } else {
      emit_loadi64(as, RID_TMP, ofs);
      emit_rre(as, S390I_AGR, RID_TMP, base);
      emit_rxy(as, 0xed60, r - RID_MIN_FPR, 0, RID_TMP, 0);
    }
  }
}

/* Add offset to pointer */
static void emit_addptr(ASMState *as, Reg r, int32_t ofs)
{
  if (ofs != 0) {
    if (ofs >= -32768 && ofs <= 32767) {
      emit_ri(as, S390I_AGHI, r, ofs);
    } else {
      emit_ril(as, S390I_LGFI, RID_TMP, ofs);
      emit_rre(as, S390I_AGR, r, RID_TMP);
    }
  }
}

/* Prefer rematerialization of BASE/L from global_State over spills */
#define emit_canremat(ref)  ((ref) <= REF_BASE)

/* Load 64-bit IR constant into register */
static void emit_loadk64(ASMState *as, Reg r, IRIns *ir)
{
  const uint64_t *k = &ir_k64(ir)->u64;
  if (*k == 0) {
    /* Zero can be done with XOR */
    if (rset_test(RSET_FPR, r)) {
      emit_rre(as, S390I_XGR, r - RID_MIN_FPR, r - RID_MIN_FPR);
    } else {
      emit_rre(as, S390I_XGR, r, r);
    }
  } else {
    emit_loadi64(as, r, (int64_t)*k);
  }
}
