static Reg ra_hintalloc(ASMState *as, IRRef ref, Reg hint, RegSet allow)
{
}
static Reg ra_alloc2(ASMState *as, IRIns *ir, RegSet allow)
{
}
static void asm_exitstub_setup(ASMState *as, ExitNo nexits)
{
}
static MCode *asm_exitstub_addr(ASMState *as, ExitNo exitno)
{
}
static void asm_guardcc(ASMState *as, int cc)
{
}
#define CONFLICT_SEARCH_LIM  31
static int noconflict(ASMState *as, IRRef ref, IROp conflict)
{
}
static int32_t asm_fuseabase(ASMState *as, IRRef ref)
{
}
static Reg asm_fuseahuref(ASMState *as, IRRef ref, int32_t *ofsp, RegSet allow)
{
}
static void asm_fusexref(ASMState *as, S390Ins si, Reg rt, IRRef ref,
                         RegSet allow, int32_t ofs)
{
}
static void asm_gencall(ASMState *as, const CCallInfo *ci, IRRef *args)
{
}
static void asm_setupresult(ASMState *as, IRIns *ir, const CCallInfo *ci)
{
}
static void asm_tvstore64(ASMState *as, Reg base, int32_t ofs, IRRef ref)
{
}
static void asm_tvptr(ASMState *as, Reg dest, IRRef ref, MSize mode)
{
}
#if LJ_HASBUFFER
static void asm_bufhdr_write(ASMState *as, Reg sb)
{
}
#endif
static void asm_tointg(ASMState *as, IRIns *ir, Reg left)
{
}
static void asm_tobit(ASMState *as, IRIns *ir)
{
}
static void asm_add(ASMState *as, IRIns *ir)
{
}
static void asm_sub(ASMState *as, IRIns *ir)
{
}
static void asm_mul(ASMState *as, IRIns *ir)
{
}
static void asm_div(ASMState *as, IRIns *ir)
{
}
static void asm_neg(ASMState *as, IRIns *ir)
{
}
static void asm_comp(ASMState *as, IRIns *ir)
{
}
static void asm_phi_break(ASMState *as, RegSet blocked, RegSet blockedby,
                          RegSet allow)
{
}
static void asm_phi_shuffle(ASMState *as)
{
}
static void asm_phi_copyspill(ASMState *as)
{
}
static void asm_ir(ASMState *as, IRIns *ir)
{
}
static Reg asm_setup_call_slots(ASMState *as, IRIns *ir, const CCallInfo *ci)
{
}
static void asm_setup_target(ASMState *as)
{
}
