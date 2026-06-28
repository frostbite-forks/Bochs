/////////////////////////////////////////////////////////////////////////
//
//  JIT opcode whitelist
//
/////////////////////////////////////////////////////////////////////////

#include "jit_include.h"

#if BX_SUPPORT_JIT

enum {
  BX_JIT_OPF_64 = 0x01,
};

static Bit8u bx_jit_opcode_class_table[BX_IA_LAST];
static Bit8u bx_jit_opcode_flags_table[BX_IA_LAST];

static void bx_jit_init_opcode_table(void)
{
  static bool inited = false;
  if (inited) return;
  inited = true;

  for (unsigned i = 0; i < BX_IA_LAST; i++) {
    bx_jit_opcode_class_table[i] = BX_JIT_OP_NONE;
    bx_jit_opcode_flags_table[i] = 0;
  }

#define JIT_ALU(op) bx_jit_opcode_class_table[op] = BX_JIT_OP_ALU_RM
#define JIT_LOGIC(op) bx_jit_opcode_class_table[op] = BX_JIT_OP_LOGIC_RM
#define JIT_MOV(op) bx_jit_opcode_class_table[op] = BX_JIT_OP_MOV_RM
#define JIT_IMM(op) bx_jit_opcode_class_table[op] = BX_JIT_OP_IMM_RM
#define JIT_UNARY(op) bx_jit_opcode_class_table[op] = BX_JIT_OP_UNARY_RM
#define JIT_BRANCH(op) bx_jit_opcode_class_table[op] = BX_JIT_OP_BRANCH
#define JIT_NOP(op) bx_jit_opcode_class_table[op] = BX_JIT_OP_NOP
#define JIT_ALU64(op) do { JIT_ALU(op); bx_jit_opcode_flags_table[op] |= BX_JIT_OPF_64; } while(0)
#define JIT_LOGIC64(op) do { JIT_LOGIC(op); bx_jit_opcode_flags_table[op] |= BX_JIT_OPF_64; } while(0)
#define JIT_MOV64(op) do { JIT_MOV(op); bx_jit_opcode_flags_table[op] |= BX_JIT_OPF_64; } while(0)
#define JIT_IMM64(op) do { JIT_IMM(op); bx_jit_opcode_flags_table[op] |= BX_JIT_OPF_64; } while(0)
#define JIT_UNARY64(op) do { JIT_UNARY(op); bx_jit_opcode_flags_table[op] |= BX_JIT_OPF_64; } while(0)
#define JIT_BRANCH64(op) do { JIT_BRANCH(op); bx_jit_opcode_flags_table[op] |= BX_JIT_OPF_64; } while(0)

  JIT_ALU(BX_IA_ADD_GdEd); JIT_ALU(BX_IA_ADD_EdGd);
  JIT_ALU(BX_IA_SUB_GdEd); JIT_ALU(BX_IA_SUB_EdGd);
  JIT_LOGIC(BX_IA_AND_GdEd); JIT_LOGIC(BX_IA_AND_EdGd);
  JIT_LOGIC(BX_IA_OR_GdEd); JIT_LOGIC(BX_IA_OR_EdGd);
  JIT_LOGIC(BX_IA_XOR_GdEd); JIT_LOGIC(BX_IA_XOR_EdGd);
  JIT_MOV(BX_IA_MOV_Op32_GdEd); JIT_MOV(BX_IA_MOV_Op32_EdGd);
  JIT_MOV(BX_IA_MOV_EdId);
  JIT_IMM(BX_IA_ADD_EdId); JIT_IMM(BX_IA_ADD_EAXId);
  JIT_IMM(BX_IA_SUB_EdId); JIT_IMM(BX_IA_AND_EdId);
  JIT_IMM(BX_IA_OR_EdId); JIT_IMM(BX_IA_XOR_EdId);
  JIT_IMM(BX_IA_ADD_EdsIb); JIT_IMM(BX_IA_SUB_EdsIb);
  JIT_IMM(BX_IA_AND_EdsIb); JIT_IMM(BX_IA_OR_EdsIb);
  JIT_IMM(BX_IA_XOR_EdsIb);
  JIT_UNARY(BX_IA_INC_Ed); JIT_UNARY(BX_IA_DEC_Ed);
  JIT_UNARY(BX_IA_NOT_Ed); JIT_UNARY(BX_IA_NEG_Ed);
  JIT_BRANCH(BX_IA_JMP_Jd); JIT_BRANCH(BX_IA_JMP_Jbd);
  JIT_BRANCH(BX_IA_JB_Jd); JIT_BRANCH(BX_IA_JBE_Jd);
  JIT_BRANCH(BX_IA_JL_Jd); JIT_BRANCH(BX_IA_JLE_Jd);
  JIT_BRANCH(BX_IA_JNB_Jd); JIT_BRANCH(BX_IA_JNBE_Jd);
  JIT_BRANCH(BX_IA_JNL_Jd); JIT_BRANCH(BX_IA_JNLE_Jd);
  JIT_BRANCH(BX_IA_JNO_Jd); JIT_BRANCH(BX_IA_JNP_Jd);
  JIT_BRANCH(BX_IA_JNS_Jd); JIT_BRANCH(BX_IA_JNZ_Jd);
  JIT_BRANCH(BX_IA_JO_Jd); JIT_BRANCH(BX_IA_JP_Jd);
  JIT_BRANCH(BX_IA_JS_Jd); JIT_BRANCH(BX_IA_JZ_Jd);

  JIT_ALU64(BX_IA_ADD_GqEq); JIT_ALU64(BX_IA_ADD_EqGq);
  JIT_ALU64(BX_IA_SUB_GqEq); JIT_ALU64(BX_IA_SUB_EqGq);
  JIT_LOGIC64(BX_IA_AND_GqEq); JIT_LOGIC64(BX_IA_AND_EqGq);
  JIT_LOGIC64(BX_IA_OR_GqEq); JIT_LOGIC64(BX_IA_OR_EqGq);
  JIT_LOGIC64(BX_IA_XOR_GqEq); JIT_LOGIC64(BX_IA_XOR_EqGq);
  JIT_MOV64(BX_IA_MOV_GqEq); JIT_MOV64(BX_IA_MOV_EqGq);
  JIT_MOV64(BX_IA_MOV_EqId);
  JIT_IMM64(BX_IA_ADD_EqId); JIT_IMM64(BX_IA_ADD_RAXId);
  JIT_IMM64(BX_IA_SUB_EqId); JIT_IMM64(BX_IA_AND_EqId);
  JIT_IMM64(BX_IA_OR_EqId); JIT_IMM64(BX_IA_XOR_EqId);
  JIT_IMM64(BX_IA_ADD_EqsIb); JIT_IMM64(BX_IA_SUB_EqsIb);
  JIT_IMM64(BX_IA_AND_EqsIb); JIT_IMM64(BX_IA_OR_EqsIb);
  JIT_IMM64(BX_IA_XOR_EqsIb);
  JIT_UNARY64(BX_IA_INC_Eq); JIT_UNARY64(BX_IA_DEC_Eq);
  JIT_UNARY64(BX_IA_NOT_Eq); JIT_UNARY64(BX_IA_NEG_Eq);
  JIT_BRANCH64(BX_IA_JMP_Jq); JIT_BRANCH64(BX_IA_JMP_Jbq);
  JIT_BRANCH64(BX_IA_JB_Jq); JIT_BRANCH64(BX_IA_JBE_Jq);
  JIT_BRANCH64(BX_IA_JL_Jq); JIT_BRANCH64(BX_IA_JLE_Jq);
  JIT_BRANCH64(BX_IA_JNB_Jq); JIT_BRANCH64(BX_IA_JNBE_Jq);
  JIT_BRANCH64(BX_IA_JNL_Jq); JIT_BRANCH64(BX_IA_JNLE_Jq);
  JIT_BRANCH64(BX_IA_JNO_Jq); JIT_BRANCH64(BX_IA_JNP_Jq);
  JIT_BRANCH64(BX_IA_JNS_Jq); JIT_BRANCH64(BX_IA_JNZ_Jq);
  JIT_BRANCH64(BX_IA_JO_Jq); JIT_BRANCH64(BX_IA_JP_Jq);
  JIT_BRANCH64(BX_IA_JS_Jq); JIT_BRANCH64(BX_IA_JZ_Jq);

  JIT_NOP(BX_IA_NOP);

#undef JIT_ALU
#undef JIT_LOGIC
#undef JIT_MOV
#undef JIT_IMM
#undef JIT_UNARY
#undef JIT_BRANCH
#undef JIT_NOP
#undef JIT_ALU64
#undef JIT_LOGIC64
#undef JIT_MOV64
#undef JIT_IMM64
#undef JIT_UNARY64
#undef JIT_BRANCH64
}

Bit8u bx_jit_get_opcode_class(unsigned ia_opcode)
{
  bx_jit_init_opcode_table();
  if (ia_opcode >= BX_IA_LAST) return BX_JIT_OP_NONE;
  return bx_jit_opcode_class_table[ia_opcode];
}

bool bx_jit_opcode_64bit(unsigned ia_opcode)
{
  bx_jit_init_opcode_table();
  if (ia_opcode >= BX_IA_LAST) return false;
  return (bx_jit_opcode_flags_table[ia_opcode] & BX_JIT_OPF_64) != 0;
}

Bit32u bx_jit_insn_opsize(unsigned ia_opcode)
{
  return bx_jit_opcode_64bit(ia_opcode) ? 8 : 4;
}

bool bx_jit_opcode_supported(unsigned ia_opcode, bool mod_c0)
{
  (void) mod_c0;
  return bx_jit_get_opcode_class(ia_opcode) != BX_JIT_OP_NONE;
}

bool bx_jit_opcode_use_helper(unsigned ia_opcode, bool mod_c0)
{
  Bit8u cls = bx_jit_get_opcode_class(ia_opcode);
  switch (cls) {
    case BX_JIT_OP_ALU_RM:
    case BX_JIT_OP_LOGIC_RM:
    case BX_JIT_OP_MOV_RM:
    case BX_JIT_OP_IMM_RM:
    case BX_JIT_OP_UNARY_RM:
      return !mod_c0;
    case BX_JIT_OP_BRANCH:
      return true;
    default:
      return false;
  }
}

#endif
