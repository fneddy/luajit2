typedef uint32_t S390Ins;

static void emit_i16(ASMState *as, S390Ins si)
{
}
static void emit_i32(ASMState *as, S390Ins si)
{
}
static void emit_i48(ASMState *as, S390Ins sih, S390Ins sil)
{
}
static void emit_rr(ASMState *as, S390Ins si, Reg r1, Reg r2)
{
}
static void emit_rre(ASMState *as, S390Ins si, Reg r1, Reg r2)
{
}
static void emit_rrf(ASMState *as, S390Ins si, Reg r1, Reg r3, Reg r2)
{
}
static void emit_rx(ASMState *as, S390Ins si, Reg r1, Reg x2, Reg b2, int32_t d2)
{
}
static void emit_rxy(ASMState *as, S390Ins si, Reg r1, Reg x2, Reg b2, int32_t d2)
{
}
static void emit_rs(ASMState *as, S390Ins si, Reg r1, Reg r3, Reg b2, int32_t d2)
{
}
static void emit_rsy(ASMState *as, S390Ins si, Reg r1, Reg r3, Reg b2, int32_t d2)
{
}
static void emit_ri(ASMState *as, S390Ins si, Reg r1, int32_t i2)
{
}
static void emit_ril(ASMState *as, S390Ins si, Reg r1, int32_t i2)
{
}
static void emit_si(ASMState *as, S390Ins si, Reg b1, int32_t d1, int32_t i2)
{
}
static void emit_loadreg(ASMState *as, Reg r, Reg base, int32_t ofs)
{
}
static void emit_storereg(ASMState *as, Reg r, Reg base, int32_t ofs)
{
}
static void emit_loadi64(ASMState *as, Reg r, int64_t i)
{
}
#define emit_loada(as, r, addr)
#define emit_getgl(as, r, field) 
#define emit_setgl(as, r, field)
#define emit_setvmstate(as, i) 
typedef MCode *MCLabel;
#define emit_label(as)  ((as)->mcp)
static void emit_branch(ASMState *as, int cc, MCode *target)
{
}
static void emit_condbranch(ASMState *as, int cc, MCode *target)
{
}
static void emit_jmp(ASMState *as, MCode *target)
{
}
static void emit_call(ASMState *as, void *target)
{
}
static void emit_movrr(ASMState *as, IRIns *ir, Reg dst, Reg src)
{
}
static void emit_loadofs(ASMState *as, IRIns *ir, Reg r, Reg base, int32_t ofs)
{
}
static void emit_storeofs(ASMState *as, IRIns *ir, Reg r, Reg base, int32_t ofs)
{
}
static void emit_addptr(ASMState *as, Reg r, int32_t ofs)
{
}
#define emit_canremat(ref)  ((ref) <= REF_BASE)
static void emit_loadk64(ASMState *as, Reg r, IRIns *ir)
{
}
