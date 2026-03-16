/*
** s390x IR assembler (SSA IR -> machine code).
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
*/

/* -- Register allocator extensions --------------------------------------- */

/* Allocate a register with a hint. */
static Reg ra_hintalloc(ASMState *as, IRRef ref, Reg hint, RegSet allow)
{
  Reg r = IR(ref)->r;
  if (ra_noreg(r)) {
    if (!ra_hashint(r) && !iscrossref(as, ref))
      ra_sethint(IR(ref)->r, hint);  /* Propagate register hint. */
    r = ra_allocref(as, ref, allow);
  }
  ra_noweak(as, r);
  return r;
}

/* Allocate two source registers for three-operand instructions. */
static Reg ra_alloc2(ASMState *as, IRIns *ir, RegSet allow)
{
  IRIns *irl = IR(ir->op1), *irr = IR(ir->op2);
  Reg left = irl->r, right = irr->r;
  if (ra_hasreg(left)) {
    ra_noweak(as, left);
    if (ra_noreg(right))
      right = ra_allocref(as, ir->op2, rset_exclude(allow, left));
    else
      ra_noweak(as, right);
  } else if (ra_hasreg(right)) {
    ra_noweak(as, right);
    left = ra_allocref(as, ir->op1, rset_exclude(allow, right));
  } else if (ra_hashint(right)) {
    right = ra_allocref(as, ir->op2, allow);
    left = ra_alloc1(as, ir->op1, rset_exclude(allow, right));
  } else {
    left = ra_allocref(as, ir->op1, allow);
    right = ra_alloc1(as, ir->op2, rset_exclude(allow, left));
  }
  return left | (right << 8);
}

/* -- Guard handling ------------------------------------------------------ */

/* Generate an exit stub group at the bottom of the reserved MCode memory. */
static MCode *asm_exitstub_gen(ASMState *as, ExitNo group)
{
  ExitNo i, groupofs = (group*EXITSTUBS_PER_GROUP) & 0xff;
  MCode *target = (MCode *)(void *)lj_vm_exit_handler;
  MCode *mxp = as->mcbot;
  MCode *mxpstart = mxp;
  
  if (mxp + (6*EXITSTUBS_PER_GROUP + 8) >= as->mctop)
    asm_mclimit(as);
  
  /* Push low byte of exitno for each exit stub. */
  /* LGHI r1, groupofs; STHG r1, 0(sp); BRCL exit_handler */
  for (i = 0; i < EXITSTUBS_PER_GROUP; i++) {
    /* Store exit number to stack and branch to handler */
    emit_ri(as, S390I_LGHI, RID_R1, (MCode)(groupofs + i));
    emit_rxy(as, 0xe324, RID_R1, 0, RID_SP, 0);  /* STG r1, 0(sp) */
    emit_ril(as, S390I_BRCL, 15, s390_jmprel(mxp, target));
  }
  
  /* Commit the code for this group. */
  lj_mcode_commitbot(as->J, mxp);
  as->mcbot = mxp;
  as->mclim = as->mcbot + MCLIM_REDZONE;
  return mxpstart;
}

/* Setup all needed exit stubs. */
static void asm_exitstub_setup(ASMState *as, ExitNo nexits)
{
  ExitNo i;
  if (nexits >= EXITSTUBS_PER_GROUP*LJ_MAX_EXITSTUBGR)
    lj_trace_err(as->J, LJ_TRERR_SNAPOV);
  for (i = 0; i < (nexits+EXITSTUBS_PER_GROUP-1)/EXITSTUBS_PER_GROUP; i++)
    if (as->J->exitstubgroup[i] == NULL)
      as->J->exitstubgroup[i] = asm_exitstub_gen(as, i);
}

/* Get exit stub address. */
static MCode *asm_exitstub_addr(ASMState *as, ExitNo exitno)
{
  return as->J->exitstubgroup[exitno/EXITSTUBS_PER_GROUP] + 
         (exitno & (EXITSTUBS_PER_GROUP-1)) * 6;
}

/* Emit conditional branch to exit for guard. */
static void asm_guardcc(ASMState *as, int cc)
{
  MCode *target = asm_exitstub_addr(as, as->snapno);
  MCode *p = as->mcp;
  if (LJ_UNLIKELY(p == as->invmcp)) {
    as->loopinv = 1;
    /* Invert condition for loop */
    emit_branch(as, cc ^ 1, target);
    return;
  }
  emit_branch(as, cc, target);
}

/* -- Memory operand fusion ----------------------------------------------- */

/* Limit linear search to this distance. Avoids O(n^2) behavior. */
#define CONFLICT_SEARCH_LIM  31

/* Check if there's no conflicting instruction between curins and ref. */
static int noconflict(ASMState *as, IRRef ref, IROp conflict)
{
  IRIns *ir = as->ir;
  IRRef i = as->curins;
  if (i > ref + CONFLICT_SEARCH_LIM)
    return 0;  /* Give up, ref is too far away. */
  while (--i > ref)
    if (ir[i].o == conflict)
      return 0;  /* Conflict found. */
  return 1;  /* Ok, no conflict. */
}

/* Fuse array base into memory operand. */
static int32_t asm_fuseabase(ASMState *as, IRRef ref)
{
  IRIns *irb = IR(ref);
  as->mrm.ofs = 0;
  if (irb->o == IR_FLOAD) {
    IRIns *ira = IR(irb->op1);
    lj_assertA(irb->op2 == IRFL_TAB_ARRAY, "expected FLOAD TAB_ARRAY");
    /* We can avoid the FLOAD of t->array for colocated arrays. */
    if (ira->o == IR_TNEW && ira->op1 <= LJ_MAX_COLOSIZE &&
        !neverfuse(as) && noconflict(as, irb->op1, IR_NEWREF)) {
      as->mrm.ofs = (int32_t)sizeof(GCtab);  /* Ofs to colocated array. */
      return irb->op1;  /* Table obj. */
    }
  } else if (irb->o == IR_ADD && irref_isk(irb->op2)) {
    /* Fuse base offset (vararg load). */
    IRIns *irk = IR(irb->op2);
    as->mrm.ofs = irk->o == IR_KINT ? irk->i : (int32_t)ir_kint64(irk)->u64;
    return irb->op1;
  }
  return ref;  /* Otherwise use the given array base. */
}

/* Fuse array/hash/upvalue reference into memory operand. */
static Reg asm_fuseahuref(ASMState *as, IRRef ref, int32_t *ofsp, RegSet allow)
{
  IRIns *ir = IR(ref);
  if (ra_noreg(ir->r)) {
    switch ((IROp)ir->o) {
    case IR_AREF:
      if (mayfuse(as, ref)) {
        IRIns *irx;
        as->mrm.base = (uint8_t)ra_alloc1(as, asm_fuseabase(as, ir->op1), allow);
        irx = IR(ir->op2);
        if (irref_isk(ir->op2)) {
          as->mrm.ofs += 8*irx->i;
          as->mrm.idx = RID_NONE;
        } else {
          rset_clear(allow, as->mrm.base);
          as->mrm.idx = (uint8_t)ra_alloc1(as, ir->op2, allow);
        }
        return RID_MRM;
      }
      break;
    case IR_HREFK:
      if (mayfuse(as, ref)) {
        as->mrm.base = (uint8_t)ra_alloc1(as, ir->op1, allow);
        as->mrm.ofs = (int32_t)(IR(ir->op2)->op2 * sizeof(Node));
        as->mrm.idx = RID_NONE;
        return RID_MRM;
      }
      break;
    case IR_UREFC:
      if (irref_isk(ir->op1)) {
        GCfunc *fn = ir_kfunc(IR(ir->op1));
        GCupval *uv = &gcref(fn->l.uvptr[(ir->op2 >> 8)])->uv;
        *ofsp = (int32_t)((intptr_t)&uv->tv - (intptr_t)J2GG(as->J));
        return RID_GL;
      }
      break;
    case IR_TMPREF:
      *ofsp = (int32_t)((intptr_t)&J2G(as->J)->tmptv - (intptr_t)J2GG(as->J));
      return RID_GL;
    default:
      break;
    }
  }
  *ofsp = 0;
  return ra_alloc1(as, ref, allow);
}

/* Fuse XLOAD/XSTORE reference into memory operand. */
static void asm_fusexref(ASMState *as, S390Ins si, Reg rt, IRRef ref,
                         RegSet allow, int32_t ofs)
{
  IRIns *ir = IR(ref);
  if (ra_noreg(ir->r) && canfuse(as, ir)) {
    if (ir->o == IR_ADD && irref_isk(ir->op2)) {
      ofs += IR(ir->op2)->i;
      ref = ir->op1;
    }
  }
  emit_loadofs(as, NULL, rt, ra_alloc1(as, ref, allow), ofs);
}

/* -- Calls --------------------------------------------------------------- */

/* Generate a call to a C function. */
static void asm_gencall(ASMState *as, const CCallInfo *ci, IRRef *args)
{
  uint32_t n, nargs = CCI_XNARGS(ci);
  int32_t ofs = STACKARG_OFS;
  uint32_t gprs = REGARG_GPRS;
  Reg fpr = REGARG_FIRSTFPR;
  
  if ((void *)ci->func)
    emit_call(as, ci->func);
  
  for (n = 0; n < nargs; n++) {
    IRRef ref = args[n];
    IRIns *ir = IR(ref);
    Reg r;
    
    /* s390x calling convention: r2-r5 for GPRs, f0,f2,f4,f6 for FPRs */
    if (irt_isfp(ir->t)) {
      r = fpr <= REGARG_LASTFPR ? fpr++ : 0;
    } else {
      r = gprs & 31; gprs >>= 5;
    }
    
    if (r) {  /* Argument is in a register. */
      if (r < RID_MAX_GPR && ref < ASMREF_TMP1) {
        emit_loadi64(as, r, ir->i);
      } else {
        lj_assertA(rset_test(as->freeset, r), "reg %d not free", r);
        if (ra_hasreg(ir->r)) {
          ra_noweak(as, ir->r);
          emit_movrr(as, ir, r, ir->r);
        } else {
          ra_allocref(as, ref, RID2RSET(r));
        }
      }
    } else {  /* Argument is on stack. */
      if (irt_isfp(ir->t)) {
        r = ra_alloc1(as, ref, RSET_FPR);
        emit_storeofs(as, ir, r, RID_SP, ofs);
      } else {
        r = ra_alloc1(as, ref, RSET_GPR);
        emit_storereg(as, r, RID_SP, ofs);
      }
      ofs += 8;
    }
    checkmclim(as);
  }
}

/* Setup result reg/sp for call. Evict scratch regs. */
static void asm_setupresult(ASMState *as, IRIns *ir, const CCallInfo *ci)
{
  RegSet drop = RSET_SCRATCH;
  if ((ci->flags & CCI_NOFPRCLOBBER))
    drop &= ~RSET_FPR;
  if (ra_hasreg(ir->r))
    rset_clear(drop, ir->r);
  ra_evictset(as, drop);
  
  if (ra_used(ir)) {
    if (irt_isfp(ir->t)) {
      ra_destreg(as, ir, RID_FPRET);
    } else {
      lj_assertA(!irt_ispri(ir->t), "PRI dest");
      ra_destreg(as, ir, RID_RET);
    }
  }
}

/* -- Type conversions ---------------------------------------------------- */

static void asm_tointg(ASMState *as, IRIns *ir, Reg left)
{
  Reg tmp = ra_scratch(as, rset_exclude(RSET_FPR, left));
  Reg dest = ra_dest(as, ir, RSET_GPR);
  asm_guardcc(as, 7);  /* NE condition */
  asm_guardcc(as, 14); /* NO condition (ordered) */
  /* Compare and convert */
  emit_rre(as, S390I_CDR, tmp - RID_MIN_FPR, left - RID_MIN_FPR);
  emit_rre(as, S390I_LDR, tmp - RID_MIN_FPR, dest - RID_MIN_FPR);
  /* Convert to int */
  emit_rre(as, S390I_LDR, dest - RID_MIN_FPR, left - RID_MIN_FPR);
}

static void asm_tobit(ASMState *as, IRIns *ir)
{
  Reg dest = ra_dest(as, ir, RSET_GPR);
  Reg tmp = ra_noreg(IR(ir->op1)->r) ?
            ra_alloc1(as, ir->op1, RSET_FPR) :
            ra_scratch(as, RSET_FPR);
  Reg right;
  emit_rre(as, S390I_LDR, tmp - RID_MIN_FPR, dest - RID_MIN_FPR);
  right = asm_fuseload(as, ir->op2, rset_exclude(RSET_FPR, tmp));
  emit_rre(as, S390I_ADR, tmp - RID_MIN_FPR, right - RID_MIN_FPR);
  ra_left(as, tmp, ir->op1);
}

/* -- FP/int arithmetic and logic operations ------------------------------ */

static void asm_add(ASMState *as, IRIns *ir)
{
  if (irt_isnum(ir->t)) {
    Reg dest = ra_dest(as, ir, RSET_FPR);
    Reg left = ra_alloc1(as, ir->op1, RSET_FPR);
    Reg right = asm_fuseload(as, ir->op2, rset_exclude(RSET_FPR, left));
    emit_rre(as, S390I_ADR, dest - RID_MIN_FPR, right - RID_MIN_FPR);
    ra_left(as, dest, ir->op1);
  } else {
    Reg dest = ra_dest(as, ir, RSET_GPR);
    Reg left = ra_alloc1(as, ir->op1, RSET_GPR);
    Reg right = asm_fuseload(as, ir->op2, rset_exclude(RSET_GPR, left));
    emit_rre(as, S390I_AGR, dest, right);
    ra_left(as, dest, ir->op1);
  }
}

static void asm_sub(ASMState *as, IRIns *ir)
{
  if (irt_isnum(ir->t)) {
    Reg dest = ra_dest(as, ir, RSET_FPR);
    Reg left = ra_alloc1(as, ir->op1, RSET_FPR);
    Reg right = asm_fuseload(as, ir->op2, rset_exclude(RSET_FPR, left));
    emit_rre(as, S390I_SDR, dest - RID_MIN_FPR, right - RID_MIN_FPR);
    ra_left(as, dest, ir->op1);
  } else {
    Reg dest = ra_dest(as, ir, RSET_GPR);
    Reg left = ra_alloc1(as, ir->op1, RSET_GPR);
    Reg right = asm_fuseload(as, ir->op2, rset_exclude(RSET_GPR, left));
    emit_rre(as, S390I_SGR, dest, right);
    ra_left(as, dest, ir->op1);
  }
}

static void asm_mul(ASMState *as, IRIns *ir)
{
  if (irt_isnum(ir->t)) {
    Reg dest = ra_dest(as, ir, RSET_FPR);
    Reg left = ra_alloc1(as, ir->op1, RSET_FPR);
    Reg right = asm_fuseload(as, ir->op2, rset_exclude(RSET_FPR, left));
    emit_rre(as, S390I_MDR, dest - RID_MIN_FPR, right - RID_MIN_FPR);
    ra_left(as, dest, ir->op1);
  } else {
    Reg dest = ra_dest(as, ir, RSET_GPR);
    Reg left = ra_alloc1(as, ir->op1, RSET_GPR);
    Reg right = asm_fuseload(as, ir->op2, rset_exclude(RSET_GPR, left));
    emit_rre(as, S390I_MGR, dest, right);
    ra_left(as, dest, ir->op1);
  }
}

static void asm_div(ASMState *as, IRIns *ir)
{
  if (irt_isnum(ir->t)) {
    Reg dest = ra_dest(as, ir, RSET_FPR);
    Reg left = ra_alloc1(as, ir->op1, RSET_FPR);
    Reg right = asm_fuseload(as, ir->op2, rset_exclude(RSET_FPR, left));
    emit_rre(as, S390I_DDR, dest - RID_MIN_FPR, right - RID_MIN_FPR);
    ra_left(as, dest, ir->op1);
  } else {
    Reg dest = ra_dest(as, ir, RSET_GPR);
    Reg left = ra_alloc1(as, ir->op1, RSET_GPR);
    Reg right = asm_fuseload(as, ir->op2, rset_exclude(RSET_GPR, left));
    emit_rre(as, S390I_DGR, dest, right);
    ra_left(as, dest, ir->op1);
  }
}

static void asm_neg(ASMState *as, IRIns *ir)
{
  if (irt_isnum(ir->t)) {
    Reg dest = ra_dest(as, ir, RSET_FPR);
    Reg left = ra_alloc1(as, ir->op1, RSET_FPR);
    emit_rre(as, S390I_LCDFR, dest - RID_MIN_FPR, left - RID_MIN_FPR);
  } else {
    Reg dest = ra_dest(as, ir, RSET_GPR);
    Reg left = ra_alloc1(as, ir->op1, RSET_GPR);
    emit_rre(as, S390I_LCGFR, dest, left);
  }
}

static void asm_comp(ASMState *as, IRIns *ir)
{
  /* Comparison and guard */
  if (irt_isnum(ir->t)) {
    Reg left = ra_alloc1(as, ir->op1, RSET_FPR);
    Reg right = asm_fuseload(as, ir->op2, rset_exclude(RSET_FPR, left));
    emit_rre(as, S390I_CDR, left - RID_MIN_FPR, right - RID_MIN_FPR);
  } else {
    Reg left = ra_alloc1(as, ir->op1, RSET_GPR);
    Reg right = asm_fuseload(as, ir->op2, rset_exclude(RSET_GPR, left));
    emit_rre(as, S390I_CGR, left, right);
  }
  
  /* Map IR comparison to condition code */
  switch (ir->o) {
  case IR_LT: asm_guardcc(as, 4); break;  /* Less than */
  case IR_GE: asm_guardcc(as, 10); break; /* Greater or equal */
  case IR_LE: asm_guardcc(as, 10); break; /* Less or equal (swap) */
  case IR_GT: asm_guardcc(as, 2); break;  /* Greater than */
  case IR_EQ: asm_guardcc(as, 8); break;  /* Equal */
  case IR_NE: asm_guardcc(as, 7); break;  /* Not equal */
  default: break;
  }
}

/* -- PHI handling -------------------------------------------------------- */

static void asm_phi_break(ASMState *as, RegSet blocked, RegSet blockedby,
                          RegSet allow)
{
  /* Break cycles in PHI register shuffling */
  RegSet work = blocked;
  while (work) {
    Reg r = rset_pickbot(work);
    rset_clear(work, r);
    if (rset_test(blockedby, r)) {
      /* Need to break cycle */
      IRRef ref = regcost_ref(as->cost[r]);
      int32_t spill = ra_spill(as, IR(ref));
      emit_storereg(as, r, RID_SP, spill);
    }
  }
}

static void asm_phi_shuffle(ASMState *as)
{
  /* Shuffle PHI registers */
  /* s390x uses 3-operand instructions, so this is simpler */
}

static void asm_phi_copyspill(ASMState *as)
{
  /* Copy spilled PHI values */
}

/* -- Main instruction dispatch ------------------------------------------- */

static void asm_ir(ASMState *as, IRIns *ir)
{
  switch ((IROp)ir->o) {
  case IR_ADD: asm_add(as, ir); break;
  case IR_SUB: asm_sub(as, ir); break;
  case IR_MUL: asm_mul(as, ir); break;
  case IR_DIV: asm_div(as, ir); break;
  case IR_NEG: asm_neg(as, ir); break;
  case IR_LT: case IR_GE: case IR_LE: case IR_GT:
  case IR_EQ: case IR_NE: asm_comp(as, ir); break;
  default:
    /* NYI: other operations */
    break;
  }
}

/* -- Setup --------------------------------------------------------------- */

static Reg asm_setup_call_slots(ASMState *as, IRIns *ir, const CCallInfo *ci)
{
  IRRef args[CCI_NARGS_MAX*2];
  int nslots;
  asm_collectargs(as, ir, ci, args);
  nslots = asm_count_call_slots(as, ci, args);
  if (nslots > as->evenspill)
    as->evenspill = nslots;
  return irt_isfp(ir->t) ? REGSP_HINT(RID_FPRET) : REGSP_HINT(RID_RET);
}

static void asm_setup_target(ASMState *as)
{
  asm_exitstub_setup(as, as->T->nsnap);
  as->mrm.base = 0;
}
