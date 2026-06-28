/////////////////////////////////////////////////////////////////////////
//
//  JIT C helpers and CPU glue
//
/////////////////////////////////////////////////////////////////////////

#define NEED_CPU_REG_SHORTCUTS 1
#include "jit_include.h"
#define LOG_THIS BX_CPU_THIS_PTR

#include "lazy_flags.h"

#if BX_SUPPORT_JIT

#if BX_DEBUGGER || BX_GDBSTUB
#include "bx_debug/debug.h"
#endif

bool bx_jit_can_compile_cpu(BX_CPU_C *cpu)
{
  (void) cpu;
  return true;
}

bool bx_jit_trace_jitable(BX_CPU_C *cpu, bxICacheEntry_c *entry)
{
  if (! bx_jit_can_compile_cpu(cpu)) return false;

  bxInstruction_c *i = entry->i;
  bxInstruction_c *last = i + entry->tlen;

  for (; i < last; i++) {
    if (i->getIaOpcode() == BX_IA_ERROR) return false;
    if (i->lockRepUsedValue() != 0) return false;
    if (! bx_jit_opcode_supported(i->getIaOpcode(), i->modC0() != 0))
      return false;
  }

  return true;
}

Bit32u BX_CPU_C::jit_finish_insn(bxInstruction_c *i)
{
  BX_CPU_THIS_PTR prev_rip = RIP;
  BX_INSTR_AFTER_EXECUTION(BX_CPU_ID, i);
  BX_CPU_THIS_PTR icount++;
  return BX_CPU_THIS_PTR async_event ? 1 : 0;
}

Bit32u BX_CPU_C::jit_invoke_one(bxInstruction_c *i)
{
  BX_CPU_THIS_PTR jit_invocation = 1;

  BX_INSTR_BEFORE_EXECUTION(BX_CPU_ID, i);
  RIP += i->ilen();
  BX_CPU_CALL_METHOD(i->execute1, (i));

  BX_CPU_THIS_PTR jit_invocation = 0;
  return jit_finish_insn(i);
}

Bit32u BX_CPU_C::jit_reg_alu(bxInstruction_c *i, Bit32u sub)
{
  Bit32u op1 = BX_READ_32BIT_REG(i->dst());
  Bit32u op2 = BX_READ_32BIT_REG(i->src1());
  Bit32u res;

  BX_INSTR_BEFORE_EXECUTION(BX_CPU_ID, i);
  RIP += i->ilen();

  if (sub)
    res = op1 - op2;
  else
    res = op1 + op2;

  BX_WRITE_32BIT_REGZ(i->dst(), res);

  if (sub) {
    SET_FLAGS_OSZAPC_SUB_32(op1, op2, res);
  } else {
    SET_FLAGS_OSZAPC_ADD_32(op1, op2, res);
  }

  BX_CLEAR_64BIT_HIGH(i->dst());
  return jit_finish_insn(i);
}

Bit32u BX_CPU_C::jit_reg_logic(bxInstruction_c *i, Bit32u op)
{
  Bit32u op1 = BX_READ_32BIT_REG(i->dst());
  Bit32u op2 = BX_READ_32BIT_REG(i->src1());
  Bit32u res = 0;

  BX_INSTR_BEFORE_EXECUTION(BX_CPU_ID, i);
  RIP += i->ilen();

  switch (op) {
    case 0: res = op1 & op2; break;
    case 1: res = op1 | op2; break;
    default: res = op1 ^ op2; break;
  }

  BX_WRITE_32BIT_REGZ(i->dst(), res);
  SET_FLAGS_OSZAPC_LOGIC_32(res);
  BX_CLEAR_64BIT_HIGH(i->dst());
  return jit_finish_insn(i);
}

Bit32u BX_CPU_C::jit_reg_mov(bxInstruction_c *i)
{
  BX_INSTR_BEFORE_EXECUTION(BX_CPU_ID, i);
  RIP += i->ilen();
  BX_WRITE_32BIT_REGZ(i->dst(), BX_READ_32BIT_REG(i->src1()));
  BX_CLEAR_64BIT_HIGH(i->dst());
  return jit_finish_insn(i);
}

Bit32u BX_CPU_C::jit_reg_imm(bxInstruction_c *i)
{
  unsigned op = i->getIaOpcode();
  Bit32u imm = i->Id();
  Bit32u dst_val = BX_READ_32BIT_REG(i->dst());
  Bit32u res;

  BX_INSTR_BEFORE_EXECUTION(BX_CPU_ID, i);
  RIP += i->ilen();

  switch (op) {
    case BX_IA_ADD_EdId:
    case BX_IA_ADD_EAXId:
    case BX_IA_ADD_EdsIb:
      res = dst_val + imm;
      SET_FLAGS_OSZAPC_ADD_32(dst_val, imm, res);
      break;
    case BX_IA_SUB_EdId:
    case BX_IA_SUB_EdsIb:
      res = dst_val - imm;
      SET_FLAGS_OSZAPC_SUB_32(dst_val, imm, res);
      break;
    case BX_IA_AND_EdId:
    case BX_IA_AND_EdsIb:
      res = dst_val & imm;
      SET_FLAGS_OSZAPC_LOGIC_32(res);
      break;
    case BX_IA_OR_EdId:
    case BX_IA_OR_EdsIb:
      res = dst_val | imm;
      SET_FLAGS_OSZAPC_LOGIC_32(res);
      break;
    case BX_IA_XOR_EdId:
    case BX_IA_XOR_EdsIb:
      res = dst_val ^ imm;
      SET_FLAGS_OSZAPC_LOGIC_32(res);
      break;
    default:
      BX_WRITE_32BIT_REGZ(i->dst(), imm);
      return jit_finish_insn(i);
  }

  BX_WRITE_32BIT_REGZ(i->dst(), res);
  BX_CLEAR_64BIT_HIGH(i->dst());
  return jit_finish_insn(i);
}

void BX_CPU_C::jit_set_flags_add32(Bit32u op1, Bit32u op2, Bit32u res)
{
  SET_FLAGS_OSZAPC_ADD_32(op1, op2, res);
}

void BX_CPU_C::jit_set_flags_sub32(Bit32u op1, Bit32u op2, Bit32u res)
{
  SET_FLAGS_OSZAPC_SUB_32(op1, op2, res);
}

void BX_CPU_C::jit_set_flags_logic32(Bit32u res)
{
  SET_FLAGS_OSZAPC_LOGIC_32(res);
}

void BX_CPU_C::jit_set_flags_inc32(Bit32u val, Bit32u res)
{
  SET_FLAGS_OSZAP_ADD_32(val, 0, res);
}

void BX_CPU_C::jit_set_flags_dec32(Bit32u val, Bit32u res)
{
  SET_FLAGS_OSZAP_SUB_32(val, 0, res);
}

Bit32u BX_CPU_C::jit_reg_unary(bxInstruction_c *i, Bit32u op)
{
  Bit32u val = BX_READ_32BIT_REG(i->dst());
  Bit32u res;

  BX_INSTR_BEFORE_EXECUTION(BX_CPU_ID, i);
  RIP += i->ilen();

  switch (op) {
    case 0:
      res = val + 1;
      SET_FLAGS_OSZAP_ADD_32(val, 0, res);
      break;
    case 1:
      res = val - 1;
      SET_FLAGS_OSZAP_SUB_32(val, 0, res);
      break;
    case 2:
      res = ~val;
      SET_FLAGS_OSZAPC_LOGIC_32(res);
      break;
    default:
      res = (Bit32u)(0 - val);
      SET_FLAGS_OSZAPC_SUB_32(0, val, res);
      break;
  }

  BX_WRITE_32BIT_REGZ(i->dst(), res);
  BX_CLEAR_64BIT_HIGH(i->dst());
  return jit_finish_insn(i);
}

bool BX_CPU_C::jit_can_fast_mov(bxInstruction_c *i, bool is_load)
{
  if (i->seg() != BX_SEG_REG_DS && i->seg() != BX_SEG_REG_ES &&
      i->seg() != BX_SEG_REG_SS && i->seg() != BX_SEG_REG_CS)
    return false;

  const bx_segment_reg_t *seg = &BX_CPU_THIS_PTR sregs[i->seg()];
  if (! seg->cache.valid) return false;
  unsigned t = seg->cache.type;
  if (t > 7) return false;

  (void) is_load;
  return true;
}

Bit32u BX_CPU_C::jit_fast_mov_load(bxInstruction_c *i)
{
  if (! jit_can_fast_mov(i, true))
    return jit_invoke_one(i);

  bx_address eaddr = BX_CPU_RESOLVE_ADDR(i);
  Bit32u val = read_virtual_dword(i->seg(), eaddr);

  BX_INSTR_BEFORE_EXECUTION(BX_CPU_ID, i);
  RIP += i->ilen();
  BX_WRITE_32BIT_REGZ(i->dst(), val);
  BX_CLEAR_64BIT_HIGH(i->dst());
  return jit_finish_insn(i);
}

Bit32u BX_CPU_C::jit_fast_mov_store(bxInstruction_c *i)
{
  if (! jit_can_fast_mov(i, false))
    return jit_invoke_one(i);

  bx_address eaddr = BX_CPU_RESOLVE_ADDR(i);
  Bit32u val = BX_READ_32BIT_REG(i->src1());

  BX_INSTR_BEFORE_EXECUTION(BX_CPU_ID, i);
  RIP += i->ilen();
  write_virtual_dword(i->seg(), eaddr, val);
  return jit_finish_insn(i);
}

extern "C" {

Bit32u bx_jit_invoke_one(BX_CPU_C *cpu, bxInstruction_c *i)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  return BX_CPU_C::jit_invoke_one(i);
#else
  return cpu->jit_invoke_one(i);
#endif
}

Bit32u bx_jit_finish_insn(BX_CPU_C *cpu, bxInstruction_c *i)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  return BX_CPU_C::jit_finish_insn(i);
#else
  return cpu->jit_finish_insn(i);
#endif
}

Bit32u bx_jit_advance_nop(BX_CPU_C *cpu, bxInstruction_c *i)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  BX_INSTR_BEFORE_EXECUTION(BX_CPU_ID, i);
  RIP += i->ilen();
  return BX_CPU_C::jit_finish_insn(i);
#else
  BX_INSTR_BEFORE_EXECUTION(BX_CPU_ID, i);
  cpu->gen_reg[BX_32BIT_REG_EIP].dword.erx += i->ilen();
  return cpu->jit_finish_insn(i);
#endif
}

Bit32u bx_jit_reg_alu(BX_CPU_C *cpu, bxInstruction_c *i, Bit32u op)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  return BX_CPU_C::jit_reg_alu(i, op);
#else
  return cpu->jit_reg_alu(i, op);
#endif
}

Bit32u bx_jit_reg_logic(BX_CPU_C *cpu, bxInstruction_c *i, Bit32u op)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  return BX_CPU_C::jit_reg_logic(i, op);
#else
  return cpu->jit_reg_logic(i, op);
#endif
}

Bit32u bx_jit_reg_mov(BX_CPU_C *cpu, bxInstruction_c *i)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  return BX_CPU_C::jit_reg_mov(i);
#else
  return cpu->jit_reg_mov(i);
#endif
}

Bit32u bx_jit_reg_imm(BX_CPU_C *cpu, bxInstruction_c *i)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  return BX_CPU_C::jit_reg_imm(i);
#else
  return cpu->jit_reg_imm(i);
#endif
}

Bit32u bx_jit_reg_unary(BX_CPU_C *cpu, bxInstruction_c *i, Bit32u op)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  return BX_CPU_C::jit_reg_unary(i, op);
#else
  return cpu->jit_reg_unary(i, op);
#endif
}

Bit32u bx_jit_fast_mov_load(BX_CPU_C *cpu, bxInstruction_c *i)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  return BX_CPU_C::jit_fast_mov_load(i);
#else
  return cpu->jit_fast_mov_load(i);
#endif
}

Bit32u bx_jit_fast_mov_store(BX_CPU_C *cpu, bxInstruction_c *i)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  return BX_CPU_C::jit_fast_mov_store(i);
#else
  return cpu->jit_fast_mov_store(i);
#endif
}

void bx_jit_set_flags_add(BX_CPU_C *cpu, Bit32u op1, Bit32u op2, Bit32u res)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  BX_CPU_C::jit_set_flags_add32(op1, op2, res);
#else
  cpu->jit_set_flags_add32(op1, op2, res);
#endif
}

void bx_jit_set_flags_sub(BX_CPU_C *cpu, Bit32u op1, Bit32u op2, Bit32u res)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  BX_CPU_C::jit_set_flags_sub32(op1, op2, res);
#else
  cpu->jit_set_flags_sub32(op1, op2, res);
#endif
}

void bx_jit_set_flags_logic(BX_CPU_C *cpu, Bit32u res)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  BX_CPU_C::jit_set_flags_logic32(res);
#else
  cpu->jit_set_flags_logic32(res);
#endif
}

void bx_jit_set_flags_inc(BX_CPU_C *cpu, Bit32u val, Bit32u res)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  BX_CPU_C::jit_set_flags_inc32(val, res);
#else
  cpu->jit_set_flags_inc32(val, res);
#endif
}

void bx_jit_set_flags_dec(BX_CPU_C *cpu, Bit32u val, Bit32u res)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  BX_CPU_C::jit_set_flags_dec32(val, res);
#else
  cpu->jit_set_flags_dec32(val, res);
#endif
}

} // extern "C"

#endif
