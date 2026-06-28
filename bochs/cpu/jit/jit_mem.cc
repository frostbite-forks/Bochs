/////////////////////////////////////////////////////////////////////////
//
//  JIT memory access helpers and linear address resolution
//
/////////////////////////////////////////////////////////////////////////

#define NEED_CPU_REG_SHORTCUTS 1
#include "jit_include.h"
#define LOG_THIS BX_CPU_THIS_PTR

#include "lazy_flags.h"

#if BX_SUPPORT_JIT

Bit64u BX_CPU_C::jit_resolve_laddr_load(bxInstruction_c *i, Bit32u len)
{
  bx_address eaddr = BX_CPU_RESOLVE_ADDR(i);
  return (Bit64u) agen_read(i->seg(), eaddr, len);
}

Bit32u BX_CPU_C::jit_mem_mov_load(bxInstruction_c *i, Bit32u op_size)
{
  if (! jit_can_fast_mov(i, true))
    return jit_invoke_one(i);

  BX_INSTR_BEFORE_EXECUTION(BX_CPU_ID, i);
  RIP += i->ilen();

  bx_address eaddr = BX_CPU_RESOLVE_ADDR(i);
  if (op_size == 8) {
    Bit64u val = read_virtual_qword(i->seg(), eaddr);
    BX_WRITE_64BIT_REG(i->dst(), val);
  } else {
    Bit32u val = read_virtual_dword(i->seg(), eaddr);
    BX_WRITE_32BIT_REGZ(i->dst(), val);
    if (! long64_mode())
      BX_CLEAR_64BIT_HIGH(i->dst());
  }

  return jit_finish_insn(i);
}

Bit32u BX_CPU_C::jit_mem_mov_store(bxInstruction_c *i, Bit32u op_size)
{
  if (! jit_can_fast_mov(i, false))
    return jit_invoke_one(i);

  BX_INSTR_BEFORE_EXECUTION(BX_CPU_ID, i);
  RIP += i->ilen();

  bx_address eaddr = BX_CPU_RESOLVE_ADDR(i);
  if (op_size == 8)
    write_virtual_qword(i->seg(), eaddr, BX_READ_64BIT_REG(i->src1()));
  else
    write_virtual_dword(i->seg(), eaddr, BX_READ_32BIT_REG(i->src1()));

  return jit_finish_insn(i);
}

Bit32u BX_CPU_C::jit_mem_alu(bxInstruction_c *i, Bit32u op_size, Bit32u sub)
{
  BX_INSTR_BEFORE_EXECUTION(BX_CPU_ID, i);
  RIP += i->ilen();

  bx_address eaddr = BX_CPU_RESOLVE_ADDR(i);

  if (op_size == 8) {
    Bit64u op1 = read_RMW_virtual_qword(i->seg(), eaddr);
    Bit64u op2 = BX_READ_64BIT_REG(i->src1());
    Bit64u res = sub ? (op1 - op2) : (op1 + op2);
    write_RMW_linear_qword(res);
    if (sub) {
      SET_FLAGS_OSZAPC_SUB_64(op1, op2, res);
    } else {
      SET_FLAGS_OSZAPC_ADD_64(op1, op2, res);
    }
  } else {
    Bit32u op1 = read_RMW_virtual_dword(i->seg(), eaddr);
    Bit32u op2 = BX_READ_32BIT_REG(i->src1());
    Bit32u res = sub ? (op1 - op2) : (op1 + op2);
    write_RMW_linear_dword(res);
    if (sub) {
      SET_FLAGS_OSZAPC_SUB_32(op1, op2, res);
    } else {
      SET_FLAGS_OSZAPC_ADD_32(op1, op2, res);
    }
  }

  return jit_finish_insn(i);
}

Bit32u BX_CPU_C::jit_mem_logic(bxInstruction_c *i, Bit32u op_size, Bit32u op)
{
  BX_INSTR_BEFORE_EXECUTION(BX_CPU_ID, i);
  RIP += i->ilen();

  bx_address eaddr = BX_CPU_RESOLVE_ADDR(i);

  if (op_size == 8) {
    Bit64u op1 = read_RMW_virtual_qword(i->seg(), eaddr);
    Bit64u op2 = BX_READ_64BIT_REG(i->src1());
    Bit64u res = 0;
    switch (op) {
      case 0: res = op1 & op2; break;
      case 1: res = op1 | op2; break;
      default: res = op1 ^ op2; break;
    }
    write_RMW_linear_qword(res);
    SET_FLAGS_OSZAPC_LOGIC_64(res);
  } else {
    Bit32u op1 = read_RMW_virtual_dword(i->seg(), eaddr);
    Bit32u op2 = BX_READ_32BIT_REG(i->src1());
    Bit32u res = 0;
    switch (op) {
      case 0: res = op1 & op2; break;
      case 1: res = op1 | op2; break;
      default: res = op1 ^ op2; break;
    }
    write_RMW_linear_dword(res);
    SET_FLAGS_OSZAPC_LOGIC_32(res);
  }

  return jit_finish_insn(i);
}

Bit32u BX_CPU_C::jit_mem_load_linear(bxInstruction_c *i, Bit64u laddr, Bit32u op_size)
{
  BX_INSTR_BEFORE_EXECUTION(BX_CPU_ID, i);
  RIP += i->ilen();

  if (op_size == 8) {
    Bit64u val = read_linear_qword(i->seg(), (bx_address) laddr);
    BX_WRITE_64BIT_REG(i->dst(), val);
  } else {
    Bit32u val = read_linear_dword(i->seg(), (bx_address) laddr);
    BX_WRITE_32BIT_REGZ(i->dst(), val);
    if (! long64_mode())
      BX_CLEAR_64BIT_HIGH(i->dst());
  }

  return jit_finish_insn(i);
}

Bit32u BX_CPU_C::jit_mem_store_linear(bxInstruction_c *i, Bit64u laddr, Bit32u op_size)
{
  BX_INSTR_BEFORE_EXECUTION(BX_CPU_ID, i);
  RIP += i->ilen();

  if (op_size == 8)
    write_linear_qword(i->seg(), (bx_address) laddr, BX_READ_64BIT_REG(i->src1()));
  else
    write_linear_dword(i->seg(), (bx_address) laddr, BX_READ_32BIT_REG(i->src1()));

  return jit_finish_insn(i);
}

void BX_CPU_C::jit_set_flags_add64(Bit64u op1, Bit64u op2, Bit64u res)
{
  SET_FLAGS_OSZAPC_ADD_64(op1, op2, res);
}

void BX_CPU_C::jit_set_flags_sub64(Bit64u op1, Bit64u op2, Bit64u res)
{
  SET_FLAGS_OSZAPC_SUB_64(op1, op2, res);
}

void BX_CPU_C::jit_set_flags_logic64(Bit64u res)
{
  SET_FLAGS_OSZAPC_LOGIC_64(res);
}

void BX_CPU_C::jit_set_flags_inc64(Bit64u val, Bit64u res)
{
  SET_FLAGS_OSZAP_ADD_64(val, 0, res);
}

void BX_CPU_C::jit_set_flags_dec64(Bit64u val, Bit64u res)
{
  SET_FLAGS_OSZAP_SUB_64(val, 0, res);
}

extern "C" {

Bit32u bx_jit_mem_mov_load(BX_CPU_C *cpu, bxInstruction_c *i, Bit32u op_size)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  return BX_CPU_C::jit_mem_mov_load(i, op_size);
#else
  return cpu->jit_mem_mov_load(i, op_size);
#endif
}

Bit32u bx_jit_mem_mov_store(BX_CPU_C *cpu, bxInstruction_c *i, Bit32u op_size)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  return BX_CPU_C::jit_mem_mov_store(i, op_size);
#else
  return cpu->jit_mem_mov_store(i, op_size);
#endif
}

Bit32u bx_jit_mem_alu(BX_CPU_C *cpu, bxInstruction_c *i, Bit32u op_size, Bit32u sub)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  return BX_CPU_C::jit_mem_alu(i, op_size, sub);
#else
  return cpu->jit_mem_alu(i, op_size, sub);
#endif
}

Bit32u bx_jit_mem_logic(BX_CPU_C *cpu, bxInstruction_c *i, Bit32u op_size, Bit32u op)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  return BX_CPU_C::jit_mem_logic(i, op_size, op);
#else
  return cpu->jit_mem_logic(i, op_size, op);
#endif
}

Bit64u bx_jit_resolve_laddr_load(BX_CPU_C *cpu, bxInstruction_c *i, Bit32u len)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  return BX_CPU_C::jit_resolve_laddr_load(i, len);
#else
  return cpu->jit_resolve_laddr_load(i, len);
#endif
}

Bit32u bx_jit_mem_load_linear(BX_CPU_C *cpu, bxInstruction_c *i, Bit64u laddr, Bit32u op_size)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  return BX_CPU_C::jit_mem_load_linear(i, laddr, op_size);
#else
  return cpu->jit_mem_load_linear(i, laddr, op_size);
#endif
}

Bit32u bx_jit_mem_store_linear(BX_CPU_C *cpu, bxInstruction_c *i, Bit64u laddr, Bit32u op_size)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  return BX_CPU_C::jit_mem_store_linear(i, laddr, op_size);
#else
  return cpu->jit_mem_store_linear(i, laddr, op_size);
#endif
}

void bx_jit_set_flags_add64(BX_CPU_C *cpu, Bit64u op1, Bit64u op2, Bit64u res)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  BX_CPU_C::jit_set_flags_add64(op1, op2, res);
#else
  cpu->jit_set_flags_add64(op1, op2, res);
#endif
}

void bx_jit_set_flags_sub64(BX_CPU_C *cpu, Bit64u op1, Bit64u op2, Bit64u res)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  BX_CPU_C::jit_set_flags_sub64(op1, op2, res);
#else
  cpu->jit_set_flags_sub64(op1, op2, res);
#endif
}

void bx_jit_set_flags_logic64(BX_CPU_C *cpu, Bit64u res)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  BX_CPU_C::jit_set_flags_logic64(res);
#else
  cpu->jit_set_flags_logic64(res);
#endif
}

void bx_jit_set_flags_inc64(BX_CPU_C *cpu, Bit64u val, Bit64u res)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  BX_CPU_C::jit_set_flags_inc64(val, res);
#else
  cpu->jit_set_flags_inc64(val, res);
#endif
}

void bx_jit_set_flags_dec64(BX_CPU_C *cpu, Bit64u val, Bit64u res)
{
#if BX_USE_CPU_SMF
  (void) cpu;
  BX_CPU_C::jit_set_flags_dec64(val, res);
#else
  cpu->jit_set_flags_dec64(val, res);
#endif
}

} // extern "C"

#endif
