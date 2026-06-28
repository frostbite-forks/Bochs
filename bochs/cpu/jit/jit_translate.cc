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
Bit32u bx_jit_finish_insn(BX_CPU_C *cpu, bxInstruction_c *i);
Bit32u bx_jit_reg_alu(BX_CPU_C *cpu, bxInstruction_c *i, Bit32u op);
Bit32u bx_jit_reg_logic(BX_CPU_C *cpu, bxInstruction_c *i, Bit32u op);
Bit32u bx_jit_reg_mov(BX_CPU_C *cpu, bxInstruction_c *i);
Bit32u bx_jit_reg_imm(BX_CPU_C *cpu, bxInstruction_c *i);
Bit32u bx_jit_reg_unary(BX_CPU_C *cpu, bxInstruction_c *i, Bit32u op);
Bit32u bx_jit_fast_mov_load(BX_CPU_C *cpu, bxInstruction_c *i);
Bit32u bx_jit_fast_mov_store(BX_CPU_C *cpu, bxInstruction_c *i);
}

#if defined(_WIN32) || defined(__CYGWIN__)
#define BX_JIT_CPU_ARG_REG 1
#define BX_JIT_REG_ARG1 2
#define BX_JIT_REG_ARG2 8
#else
#define BX_JIT_CPU_ARG_REG 7
#define BX_JIT_REG_ARG1 6
#define BX_JIT_REG_ARG2 2
#endif

#define BX_HOST_RAX 0
#define BX_HOST_RBX 3

static void emit_async_check(bx_jit_x64_emitter *em, Bit8u **exit_async_ptr)
{
  em->mov_reg32_mem32(BX_HOST_RAX, BX_HOST_RBX, bx_jit_cpu_layout::async_event_offset());
  em->test_reg64_reg64(BX_HOST_RAX, BX_HOST_RAX);
  em->jcc8(0x05, 0);
  *exit_async_ptr = em->get_ptr() - 1;
}

static void patch_jcc8(Bit8u *patch, Bit8u *target)
{
  Bit8s rel = (Bit8s)(target - (patch + 1));
  *patch = (Bit8u) rel;
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

static void emit_call3(bx_jit_x64_emitter *em, void *fn, bxInstruction_c *i, Bit32u op)
{
#if defined(_WIN32) || defined(__CYGWIN__)
  em->mov_reg64_imm64(8, (Bit64u) op);
  em->mov_reg64_imm64(2, (Bit64u)(uintptr_t)i);
  em->mov_reg64_reg64(1, BX_HOST_RBX);
  em->mov_reg64_imm64(BX_HOST_RAX, (Bit64u)(uintptr_t)fn);
  em->call_reg64(BX_HOST_RAX);
#else
  em->mov_reg64_imm64(BX_JIT_REG_ARG2, (Bit64u) op);
  em->mov_reg64_imm64(BX_JIT_REG_ARG1, (Bit64u)(uintptr_t)i);
  em->mov_reg64_reg64(BX_JIT_CPU_ARG_REG, BX_HOST_RBX);
  em->mov_reg64_imm64(BX_HOST_RAX, (Bit64u)(uintptr_t)fn);
  em->call_reg64(BX_HOST_RAX);
#endif
}

static void emit_test_result(bx_jit_x64_emitter *em, Bit8u **early_exit)
{
  em->test_reg64_reg64(BX_HOST_RAX, BX_HOST_RAX);
  em->jcc8(0x05, 0);
  *early_exit = em->get_ptr() - 1;
}

static bool emit_insn(bx_jit_x64_emitter *em, bxInstruction_c *i)
{
  unsigned op = i->getIaOpcode();
  bool mod_c0 = i->modC0() != 0;
  Bit8u cls = bx_jit_get_opcode_class(op);

  if (bx_jit_opcode_use_helper(op, mod_c0)) {
    if (cls == BX_JIT_OP_MOV_RM && op == BX_IA_MOV_Op32_GdEd && !mod_c0) {
      emit_call2(em, (void *) bx_jit_fast_mov_load, i);
      return true;
    }
    if (cls == BX_JIT_OP_MOV_RM && op == BX_IA_MOV_Op32_EdGd && !mod_c0) {
      emit_call2(em, (void *) bx_jit_fast_mov_store, i);
      return true;
    }
    emit_call2(em, (void *) bx_jit_invoke_one, i);
    return true;
  }

  switch (cls) {
    case BX_JIT_OP_ALU_RM:
      emit_call3(em, (void *) bx_jit_reg_alu, i, (op == BX_IA_SUB_GdEd || op == BX_IA_SUB_EdGd) ? 1 : 0);
      return true;
    case BX_JIT_OP_LOGIC_RM:
      if (op == BX_IA_AND_GdEd || op == BX_IA_AND_EdGd)
        emit_call3(em, (void *) bx_jit_reg_logic, i, 0);
      else if (op == BX_IA_OR_GdEd || op == BX_IA_OR_EdGd)
        emit_call3(em, (void *) bx_jit_reg_logic, i, 1);
      else
        emit_call3(em, (void *) bx_jit_reg_logic, i, 2);
      return true;
    case BX_JIT_OP_MOV_RM:
      emit_call2(em, (void *) bx_jit_reg_mov, i);
      return true;
    case BX_JIT_OP_IMM_RM:
      emit_call2(em, (void *) bx_jit_reg_imm, i);
      return true;
    case BX_JIT_OP_UNARY_RM:
      if (op == BX_IA_INC_Ed) emit_call3(em, (void *) bx_jit_reg_unary, i, 0);
      else if (op == BX_IA_DEC_Ed) emit_call3(em, (void *) bx_jit_reg_unary, i, 1);
      else if (op == BX_IA_NOT_Ed) emit_call3(em, (void *) bx_jit_reg_unary, i, 2);
      else emit_call3(em, (void *) bx_jit_reg_unary, i, 3);
      return true;
    case BX_JIT_OP_NOP:
      emit_call2(em, (void *) bx_jit_advance_nop, i);
      return true;
    default:
      return false;
  }
}

bool bx_jit_compile_trace(BX_CPU_C *cpu, bxICacheEntry_c *entry, bx_jit_block *block)
{
  (void) cpu;

  bx_jit_x64_emitter em(&bx_jit_codebuf);
  if (! em.get_start()) return false;

  Bit8u *prologue_end = em.get_ptr();

  em.push_reg(5);
  em.mov_reg64_reg64(5, 4);
  em.push_reg(BX_HOST_RBX);
  em.mov_reg64_reg64(BX_HOST_RBX, BX_JIT_CPU_ARG_REG);

  bxInstruction_c *i = entry->i;
  bxInstruction_c *last = i + entry->tlen;
  Bit8u *exit_async = NULL;
  Bit8u *early_exits[BX_MAX_TRACE_LENGTH];
  unsigned n_early = 0;

  for (; i < last; i++) {
    Bit8u *async_patch = NULL;
    emit_async_check(&em, &async_patch);

    if (! emit_insn(&em, i)) return false;

    Bit8u *done_patch = NULL;
    emit_test_result(&em, &done_patch);
    early_exits[n_early++] = done_patch;

    patch_jcc8(async_patch, em.get_ptr());
  }

  em.mov_reg32_imm32(BX_HOST_RAX, 0);
  em.pop_reg(BX_HOST_RBX);
  em.pop_reg(5);
  em.ret();

  exit_async = em.get_ptr();
  em.mov_reg32_imm32(BX_HOST_RAX, 1);
  em.pop_reg(BX_HOST_RBX);
  em.pop_reg(5);
  em.ret();

  for (unsigned n = 0; n < n_early; n++)
    patch_jcc8(early_exits[n], exit_async);

  (void) prologue_end;

  bx_jit_codebuf.seal_page(em.get_start(), em.size());

  block->func = (bx_jit_trace_func_t) em.get_start();
  block->compiled = true;
  block->jitable = true;
  return true;
}

#endif
