/////////////////////////////////////////////////////////////////////////
//
//  JIT trace translator
//
/////////////////////////////////////////////////////////////////////////

#define NEED_CPU_REG_SHORTCUTS 1
#include "jit_include.h"

#if BX_SUPPORT_JIT

extern "C" {
Bit32u bx_jit_invoke_one(BX_CPU_C *cpu, bxInstruction_c *i);
Bit32u bx_jit_advance_nop(BX_CPU_C *cpu, bxInstruction_c *i);
Bit32u bx_jit_reg_alu(BX_CPU_C *cpu, bxInstruction_c *i, Bit32u op);
Bit32u bx_jit_reg_logic(BX_CPU_C *cpu, bxInstruction_c *i, Bit32u op);
Bit32u bx_jit_reg_mov(BX_CPU_C *cpu, bxInstruction_c *i);
Bit32u bx_jit_reg_imm(BX_CPU_C *cpu, bxInstruction_c *i);
Bit32u bx_jit_reg_unary(BX_CPU_C *cpu, bxInstruction_c *i, Bit32u op);
Bit32u bx_jit_mem_mov_load(BX_CPU_C *cpu, bxInstruction_c *i, Bit32u op_size);
Bit32u bx_jit_mem_mov_store(BX_CPU_C *cpu, bxInstruction_c *i, Bit32u op_size);
Bit32u bx_jit_mem_alu(BX_CPU_C *cpu, bxInstruction_c *i, Bit32u op_size, Bit32u sub);
Bit32u bx_jit_mem_logic(BX_CPU_C *cpu, bxInstruction_c *i, Bit32u op_size, Bit32u op);
void bx_jit_set_flags_add(BX_CPU_C *cpu, Bit32u op1, Bit32u op2, Bit32u res);
void bx_jit_set_flags_sub(BX_CPU_C *cpu, Bit32u op1, Bit32u op2, Bit32u res);
void bx_jit_set_flags_logic(BX_CPU_C *cpu, Bit32u res);
void bx_jit_set_flags_inc(BX_CPU_C *cpu, Bit32u val, Bit32u res);
void bx_jit_set_flags_dec(BX_CPU_C *cpu, Bit32u val, Bit32u res);
void bx_jit_set_flags_add64(BX_CPU_C *cpu, Bit64u op1, Bit64u op2, Bit64u res);
void bx_jit_set_flags_sub64(BX_CPU_C *cpu, Bit64u op1, Bit64u op2, Bit64u res);
void bx_jit_set_flags_logic64(BX_CPU_C *cpu, Bit64u res);
void bx_jit_set_flags_inc64(BX_CPU_C *cpu, Bit64u val, Bit64u res);
void bx_jit_set_flags_dec64(BX_CPU_C *cpu, Bit64u val, Bit64u res);
}

#if defined(_WIN32) || defined(__CYGWIN__)
#define BX_JIT_CPU_ARG_REG 1
#define BX_JIT_REG_ARG1 2
#define BX_JIT_REG_ARG2 8
#define BX_JIT_REG_ARG3 9
#else
#define BX_JIT_CPU_ARG_REG 7
#define BX_JIT_REG_ARG1 6
#define BX_JIT_REG_ARG2 2
#define BX_JIT_REG_ARG3 1
#endif

#define BX_HOST_RAX 0
#define BX_HOST_RCX 1
#define BX_HOST_RBX 3
#define BX_HOST_R10 10
#define BX_HOST_R11 11

static void patch_jcc8(Bit8u *patch, Bit8u *target)
{
  Bit8s rel = (Bit8s)(target - (patch + 1));
  *patch = (Bit8u) rel;
}

static void emit_advance(bx_jit_x64_emitter *em, unsigned ilen)
{
  em->add_mem64_imm8(BX_HOST_RBX, bx_jit_cpu_layout::rip_offset(), (Bit8u) ilen);
  em->add_mem64_imm8(BX_HOST_RBX, bx_jit_cpu_layout::icount_offset(), 1);
}

static void emit_call2(bx_jit_x64_emitter *em, void *fn, bxInstruction_c *i)
{
#if defined(_WIN32) || defined(__CYGWIN__)
  em->mov_reg64_imm64(2, (Bit64u)(uintptr_t)i);
  em->mov_reg64_reg64(1, BX_HOST_RBX);
  em->mov_reg64_imm64(BX_HOST_RAX, (Bit64u)(uintptr_t)fn);
  em->call_reg64(BX_HOST_RAX);
#else
  em->mov_reg64_imm64(BX_JIT_REG_ARG1, (Bit64u)(uintptr_t)i);
  em->mov_reg64_reg64(BX_JIT_CPU_ARG_REG, BX_HOST_RBX);
  em->mov_reg64_imm64(BX_HOST_RAX, (Bit64u)(uintptr_t)fn);
  em->call_reg64(BX_HOST_RAX);
#endif
}

static void emit_call3_ptr_u32(bx_jit_x64_emitter *em, void *fn,
    bxInstruction_c *i, Bit32u arg)
{
#if defined(_WIN32) || defined(__CYGWIN__)
  em->mov_reg64_reg64(1, BX_HOST_RBX);
  em->mov_reg64_imm64(2, (Bit64u)(uintptr_t)i);
  em->mov_reg32_imm32(8, arg);
#else
  em->mov_reg64_reg64(BX_JIT_CPU_ARG_REG, BX_HOST_RBX);
  em->mov_reg64_imm64(BX_JIT_REG_ARG1, (Bit64u)(uintptr_t)i);
  em->mov_reg32_imm32(BX_JIT_REG_ARG2, arg);
#endif
  em->mov_reg64_imm64(BX_HOST_RAX, (Bit64u)(uintptr_t)fn);
  em->call_reg64(BX_HOST_RAX);
}

static void emit_call4_ptr_u32_u32(bx_jit_x64_emitter *em, void *fn,
    bxInstruction_c *i, Bit32u a, Bit32u b)
{
#if defined(_WIN32) || defined(__CYGWIN__)
  em->mov_reg64_reg64(1, BX_HOST_RBX);
  em->mov_reg64_imm64(2, (Bit64u)(uintptr_t)i);
  em->mov_reg32_imm32(8, a);
  em->mov_reg32_imm32(9, b);
#else
  em->mov_reg64_reg64(BX_JIT_CPU_ARG_REG, BX_HOST_RBX);
  em->mov_reg64_imm64(BX_JIT_REG_ARG1, (Bit64u)(uintptr_t)i);
  em->mov_reg32_imm32(BX_JIT_REG_ARG2, a);
  em->mov_reg32_imm32(BX_JIT_REG_ARG3, b);
#endif
  em->mov_reg64_imm64(BX_HOST_RAX, (Bit64u)(uintptr_t)fn);
  em->call_reg64(BX_HOST_RAX);
}

static void emit_call3_u32(bx_jit_x64_emitter *em, void *fn, int a_reg, int b_reg)
{
#if defined(_WIN32) || defined(__CYGWIN__)
  em->mov_reg64_reg64(1, BX_HOST_RBX);
  em->mov_reg64_reg64(2, a_reg);
  em->mov_reg64_reg64(8, b_reg);
#else
  em->mov_reg64_reg64(BX_JIT_CPU_ARG_REG, BX_HOST_RBX);
  em->mov_reg64_reg64(BX_JIT_REG_ARG1, a_reg);
  em->mov_reg64_reg64(BX_JIT_REG_ARG2, b_reg);
#endif
  em->mov_reg64_imm64(BX_HOST_RAX, (Bit64u)(uintptr_t)fn);
  em->call_reg64(BX_HOST_RAX);
}

static void emit_call4_u32(bx_jit_x64_emitter *em, void *fn,
    int op1_reg, int op2_reg, int res_reg)
{
#if defined(_WIN32) || defined(__CYGWIN__)
  em->mov_reg64_reg64(1, BX_HOST_RBX);
  em->mov_reg64_reg64(2, op1_reg);
  em->mov_reg64_reg64(8, op2_reg);
  em->mov_reg64_reg64(9, res_reg);
#else
  em->mov_reg64_reg64(BX_JIT_CPU_ARG_REG, BX_HOST_RBX);
  em->mov_reg64_reg64(BX_JIT_REG_ARG1, op1_reg);
  em->mov_reg64_reg64(BX_JIT_REG_ARG2, op2_reg);
  em->mov_reg64_reg64(BX_JIT_REG_ARG3, res_reg);
#endif
  em->mov_reg64_imm64(BX_HOST_RAX, (Bit64u)(uintptr_t)fn);
  em->call_reg64(BX_HOST_RAX);
}

static void emit_call2_u32(bx_jit_x64_emitter *em, void *fn, int val_reg)
{
#if defined(_WIN32) || defined(__CYGWIN__)
  em->mov_reg64_reg64(1, BX_HOST_RBX);
  em->mov_reg64_reg64(2, val_reg);
#else
  em->mov_reg64_reg64(BX_JIT_CPU_ARG_REG, BX_HOST_RBX);
  em->mov_reg64_reg64(BX_JIT_REG_ARG1, val_reg);
#endif
  em->mov_reg64_imm64(BX_HOST_RAX, (Bit64u)(uintptr_t)fn);
  em->call_reg64(BX_HOST_RAX);
}

static void emit_test_async(bx_jit_x64_emitter *em, Bit8u **exit_patch)
{
  em->test_reg64_reg64(BX_HOST_RAX, BX_HOST_RAX);
  em->jcc8(0x05, 0);
  *exit_patch = em->get_ptr() - 1;
}

static bool is_sub_opcode(unsigned op)
{
  return (op == BX_IA_SUB_GdEd || op == BX_IA_SUB_EdGd ||
          op == BX_IA_SUB_GqEq || op == BX_IA_SUB_EqGq ||
          op == BX_IA_SUB_EdId || op == BX_IA_SUB_EdsIb ||
          op == BX_IA_SUB_EqId || op == BX_IA_SUB_EqsIb);
}

static bool can_emit_inline(bxInstruction_c *i)
{
  if (! i->modC0()) return false;
  Bit8u cls = bx_jit_get_opcode_class(i->getIaOpcode());
  switch (cls) {
    case BX_JIT_OP_ALU_RM:
    case BX_JIT_OP_LOGIC_RM:
    case BX_JIT_OP_MOV_RM:
    case BX_JIT_OP_IMM_RM:
    case BX_JIT_OP_UNARY_RM:
    case BX_JIT_OP_NOP:
      return true;
    default:
      return false;
  }
}

static bool can_emit_mem(bxInstruction_c *i)
{
  if (i->modC0()) return false;
  return bx_jit_opcode_use_helper(i->getIaOpcode(), false);
}

static bool emit_inline_insn(bx_jit_x64_emitter *em, bxInstruction_c *i)
{
  unsigned op = i->getIaOpcode();
  Bit8u cls = bx_jit_get_opcode_class(op);
  unsigned dst = i->dst();
  unsigned src = i->src1();
  unsigned dst_off = bx_jit_cpu_layout::gen_reg_rrx(dst);
  unsigned src_off = bx_jit_cpu_layout::gen_reg_rrx(src);
  unsigned dst_erx = bx_jit_cpu_layout::gen_reg_erx(dst);
  Bit32u op_size = bx_jit_insn_opsize(op);
  bool is64 = (op_size == 8);

  switch (cls) {
    case BX_JIT_OP_NOP:
      emit_advance(em, i->ilen());
      return true;

    case BX_JIT_OP_MOV_RM:
      if (op == BX_IA_MOV_EdId || op == BX_IA_MOV_EqId) {
        if (is64) {
          em->mov_reg64_imm64(BX_HOST_RAX, i->Id());
          em->mov_mem64_reg64(BX_HOST_RBX, dst_off, BX_HOST_RAX);
        } else {
          em->mov_reg32_imm32(BX_HOST_RAX, i->Id());
          em->mov_mem64_reg64(BX_HOST_RBX, dst_off, BX_HOST_RAX);
        }
      } else {
        em->mov_reg64_mem64(BX_HOST_RAX, BX_HOST_RBX, src_off);
        em->mov_mem64_reg64(BX_HOST_RBX, dst_off, BX_HOST_RAX);
      }
      emit_advance(em, i->ilen());
      return true;

    case BX_JIT_OP_ALU_RM:
      em->mov_reg64_mem64(BX_HOST_RAX, BX_HOST_RBX, dst_off);
      em->mov_reg64_mem64(BX_HOST_RCX, BX_HOST_RBX, src_off);
      em->mov_reg64_reg64(BX_HOST_R10, BX_HOST_RAX);
      em->mov_reg64_reg64(BX_HOST_R11, BX_HOST_RCX);
      if (is_sub_opcode(op))
        em->sub_reg64_reg64(BX_HOST_RAX, BX_HOST_RCX);
      else
        em->add_reg64_reg64(BX_HOST_RAX, BX_HOST_RCX);
      em->mov_mem64_reg64(BX_HOST_RBX, dst_off, BX_HOST_RAX);
      if (is64) {
        if (is_sub_opcode(op))
          emit_call4_u32(em, (void *) bx_jit_set_flags_sub64,
              BX_HOST_R10, BX_HOST_R11, BX_HOST_RAX);
        else
          emit_call4_u32(em, (void *) bx_jit_set_flags_add64,
              BX_HOST_R10, BX_HOST_R11, BX_HOST_RAX);
      } else {
        if (is_sub_opcode(op))
          emit_call4_u32(em, (void *) bx_jit_set_flags_sub,
              BX_HOST_R10, BX_HOST_R11, BX_HOST_RAX);
        else
          emit_call4_u32(em, (void *) bx_jit_set_flags_add,
              BX_HOST_R10, BX_HOST_R11, BX_HOST_RAX);
      }
      emit_advance(em, i->ilen());
      return true;

    case BX_JIT_OP_LOGIC_RM:
      em->mov_reg64_mem64(BX_HOST_RAX, BX_HOST_RBX, dst_off);
      em->mov_reg64_mem64(BX_HOST_RCX, BX_HOST_RBX, src_off);
      if (op == BX_IA_AND_GdEd || op == BX_IA_AND_EdGd ||
          op == BX_IA_AND_GqEq || op == BX_IA_AND_EqGq)
        em->and_reg64_reg64(BX_HOST_RAX, BX_HOST_RCX);
      else if (op == BX_IA_OR_GdEd || op == BX_IA_OR_EdGd ||
               op == BX_IA_OR_GqEq || op == BX_IA_OR_EqGq)
        em->or_reg64_reg64(BX_HOST_RAX, BX_HOST_RCX);
      else
        em->xor_reg64_reg64(BX_HOST_RAX, BX_HOST_RCX);
      em->mov_mem64_reg64(BX_HOST_RBX, dst_off, BX_HOST_RAX);
      if (is64)
        emit_call2_u32(em, (void *) bx_jit_set_flags_logic64, BX_HOST_RAX);
      else
        emit_call2_u32(em, (void *) bx_jit_set_flags_logic, BX_HOST_RAX);
      emit_advance(em, i->ilen());
      return true;

    case BX_JIT_OP_IMM_RM: {
      Bit64u imm;
      if (op == BX_IA_ADD_EdsIb || op == BX_IA_SUB_EdsIb ||
          op == BX_IA_AND_EdsIb || op == BX_IA_OR_EdsIb || op == BX_IA_XOR_EdsIb ||
          op == BX_IA_ADD_EqsIb || op == BX_IA_SUB_EqsIb ||
          op == BX_IA_AND_EqsIb || op == BX_IA_OR_EqsIb || op == BX_IA_XOR_EqsIb)
        imm = (Bit64u)(Bit64s)(Bit8s) i->Ib();
      else
        imm = i->Id();
      em->mov_reg64_mem64(BX_HOST_RAX, BX_HOST_RBX, dst_off);
      em->mov_reg64_reg64(BX_HOST_R10, BX_HOST_RAX);
      em->mov_reg64_imm64(BX_HOST_RCX, imm);
      if (is_sub_opcode(op))
        em->sub_reg64_reg64(BX_HOST_RAX, BX_HOST_RCX);
      else if (op == BX_IA_AND_EdId || op == BX_IA_AND_EdsIb ||
               op == BX_IA_AND_EqId || op == BX_IA_AND_EqsIb)
        em->and_reg64_reg64(BX_HOST_RAX, BX_HOST_RCX);
      else if (op == BX_IA_OR_EdId || op == BX_IA_OR_EdsIb ||
               op == BX_IA_OR_EqId || op == BX_IA_OR_EqsIb)
        em->or_reg64_reg64(BX_HOST_RAX, BX_HOST_RCX);
      else if (op == BX_IA_XOR_EdId || op == BX_IA_XOR_EdsIb ||
               op == BX_IA_XOR_EqId || op == BX_IA_XOR_EqsIb)
        em->xor_reg64_reg64(BX_HOST_RAX, BX_HOST_RCX);
      else
        em->add_reg64_reg64(BX_HOST_RAX, BX_HOST_RCX);
      em->mov_mem64_reg64(BX_HOST_RBX, dst_off, BX_HOST_RAX);
      if (is_sub_opcode(op)) {
        if (is64)
          emit_call4_u32(em, (void *) bx_jit_set_flags_sub64,
              BX_HOST_R10, BX_HOST_RCX, BX_HOST_RAX);
        else
          emit_call4_u32(em, (void *) bx_jit_set_flags_sub,
              BX_HOST_R10, BX_HOST_RCX, BX_HOST_RAX);
      } else if (op == BX_IA_AND_EdId || op == BX_IA_AND_EdsIb ||
                 op == BX_IA_OR_EdId || op == BX_IA_OR_EdsIb ||
                 op == BX_IA_XOR_EdId || op == BX_IA_XOR_EdsIb ||
                 op == BX_IA_AND_EqId || op == BX_IA_AND_EqsIb ||
                 op == BX_IA_OR_EqId || op == BX_IA_OR_EqsIb ||
                 op == BX_IA_XOR_EqId || op == BX_IA_XOR_EqsIb) {
        if (is64)
          emit_call2_u32(em, (void *) bx_jit_set_flags_logic64, BX_HOST_RAX);
        else
          emit_call2_u32(em, (void *) bx_jit_set_flags_logic, BX_HOST_RAX);
      } else {
        if (is64)
          emit_call4_u32(em, (void *) bx_jit_set_flags_add64,
              BX_HOST_R10, BX_HOST_RCX, BX_HOST_RAX);
        else
          emit_call4_u32(em, (void *) bx_jit_set_flags_add,
              BX_HOST_R10, BX_HOST_RCX, BX_HOST_RAX);
      }
      emit_advance(em, i->ilen());
      return true;
    }

    case BX_JIT_OP_UNARY_RM:
      em->mov_reg64_mem64(BX_HOST_RAX, BX_HOST_RBX, dst_off);
      em->mov_reg64_reg64(BX_HOST_R10, BX_HOST_RAX);
      if (op == BX_IA_INC_Ed || op == BX_IA_INC_Eq) {
        if (is64)
          em->add_reg64_imm8(BX_HOST_RAX, 1);
        else
          em->add_reg32_imm8(BX_HOST_RAX, 1);
        em->mov_mem64_reg64(BX_HOST_RBX, dst_off, BX_HOST_RAX);
        if (is64)
          emit_call3_u32(em, (void *) bx_jit_set_flags_inc64, BX_HOST_R10, BX_HOST_RAX);
        else
          emit_call3_u32(em, (void *) bx_jit_set_flags_inc, BX_HOST_R10, BX_HOST_RAX);
      } else if (op == BX_IA_DEC_Ed || op == BX_IA_DEC_Eq) {
        if (is64)
          em->sub_reg64_imm8(BX_HOST_RAX, 1);
        else
          em->sub_reg32_imm8(BX_HOST_RAX, 1);
        em->mov_mem64_reg64(BX_HOST_RBX, dst_off, BX_HOST_RAX);
        if (is64)
          emit_call3_u32(em, (void *) bx_jit_set_flags_dec64, BX_HOST_R10, BX_HOST_RAX);
        else
          emit_call3_u32(em, (void *) bx_jit_set_flags_dec, BX_HOST_R10, BX_HOST_RAX);
      } else if (op == BX_IA_NOT_Ed || op == BX_IA_NOT_Eq) {
        em->not_reg32_mem32(BX_HOST_RBX, dst_erx);
        em->mov_reg64_mem64(BX_HOST_RAX, BX_HOST_RBX, dst_off);
        if (is64)
          emit_call2_u32(em, (void *) bx_jit_set_flags_logic64, BX_HOST_RAX);
        else
          emit_call2_u32(em, (void *) bx_jit_set_flags_logic, BX_HOST_RAX);
      } else {
        em->xor_reg64_reg64(BX_HOST_R11, BX_HOST_R11);
        em->neg_reg32_mem32(BX_HOST_RBX, dst_erx);
        em->mov_reg64_mem64(BX_HOST_RAX, BX_HOST_RBX, dst_off);
        if (is64)
          emit_call4_u32(em, (void *) bx_jit_set_flags_sub64,
              BX_HOST_R11, BX_HOST_R10, BX_HOST_RAX);
        else
          emit_call4_u32(em, (void *) bx_jit_set_flags_sub,
              BX_HOST_R11, BX_HOST_R10, BX_HOST_RAX);
      }
      emit_advance(em, i->ilen());
      return true;

    default:
      return false;
  }
}

static bool emit_mem_insn(bx_jit_x64_emitter *em, bxInstruction_c *i, Bit8u **async_patch)
{
  unsigned op = i->getIaOpcode();
  Bit8u cls = bx_jit_get_opcode_class(op);
  Bit32u op_size = bx_jit_insn_opsize(op);

  switch (cls) {
    case BX_JIT_OP_MOV_RM:
      if (op == BX_IA_MOV_Op32_GdEd || op == BX_IA_MOV_GqEq)
        emit_call3_ptr_u32(em, (void *) bx_jit_mem_mov_load, i, op_size);
      else if (op == BX_IA_MOV_Op32_EdGd || op == BX_IA_MOV_EqGq)
        emit_call3_ptr_u32(em, (void *) bx_jit_mem_mov_store, i, op_size);
      else
        emit_call2(em, (void *) bx_jit_invoke_one, i);
      break;
    case BX_JIT_OP_ALU_RM:
      emit_call4_ptr_u32_u32(em, (void *) bx_jit_mem_alu, i, op_size,
          is_sub_opcode(op) ? 1 : 0);
      break;
    case BX_JIT_OP_LOGIC_RM:
      if (op == BX_IA_AND_GdEd || op == BX_IA_AND_EdGd ||
          op == BX_IA_AND_GqEq || op == BX_IA_AND_EqGq)
        emit_call4_ptr_u32_u32(em, (void *) bx_jit_mem_logic, i, op_size, 0);
      else if (op == BX_IA_OR_GdEd || op == BX_IA_OR_EdGd ||
               op == BX_IA_OR_GqEq || op == BX_IA_OR_EqGq)
        emit_call4_ptr_u32_u32(em, (void *) bx_jit_mem_logic, i, op_size, 1);
      else
        emit_call4_ptr_u32_u32(em, (void *) bx_jit_mem_logic, i, op_size, 2);
      break;
    default:
      emit_call2(em, (void *) bx_jit_invoke_one, i);
      break;
  }

  emit_test_async(em, async_patch);
  return true;
}

static bool emit_slow_insn(bx_jit_x64_emitter *em, bxInstruction_c *i, Bit8u **async_patch)
{
  if (can_emit_mem(i))
    return emit_mem_insn(em, i, async_patch);

  emit_call2(em, (void *) bx_jit_invoke_one, i);
  emit_test_async(em, async_patch);
  return true;
}

bool bx_jit_compile_trace(BX_CPU_C *cpu, bxICacheEntry_c *entry, bx_jit_block *block)
{
  (void) cpu;

  bx_jit_x64_emitter em(&bx_jit_codebuf);
  if (! em.get_start()) return false;

  em.push_reg(5);
  em.mov_reg64_reg64(5, 4);
  em.push_reg(BX_HOST_RBX);
  em.mov_reg64_reg64(BX_HOST_RBX, BX_JIT_CPU_ARG_REG);

  bxInstruction_c *i = entry->i;
  bxInstruction_c *last = i + entry->tlen;
  Bit8u *exit_async = NULL;
  Bit8u *async_patches[BX_MAX_TRACE_LENGTH];
  unsigned n_async = 0;
  unsigned n_fast = 0;

  for (; i < last; i++) {
    if (can_emit_inline(i)) {
      if (! emit_inline_insn(&em, i))
        return false;
      n_fast++;
    } else {
      if (! emit_slow_insn(&em, i, &async_patches[n_async++]))
        return false;
      if (can_emit_mem(i))
        n_fast++;
    }
  }

  if (n_fast == 0)
    return false;

  em.mov_reg64_mem64(BX_HOST_RAX, BX_HOST_RBX, bx_jit_cpu_layout::rip_offset());
  em.mov_mem64_reg64(BX_HOST_RBX, bx_jit_cpu_layout::prev_rip_offset(), BX_HOST_RAX);

  em.mov_reg32_imm32(BX_HOST_RAX, 0);
  em.pop_reg(BX_HOST_RBX);
  em.pop_reg(5);
  em.ret();

  exit_async = em.get_ptr();
  em.mov_reg32_imm32(BX_HOST_RAX, 1);
  em.pop_reg(BX_HOST_RBX);
  em.pop_reg(5);
  em.ret();

  for (unsigned n = 0; n < n_async; n++)
    patch_jcc8(async_patches[n], exit_async);

  bx_jit_codebuf.seal_page(em.get_start(), em.size());

  block->func = (bx_jit_trace_func_t) em.get_start();
  block->compiled = true;
  block->jitable = true;
  return true;
}

#endif
