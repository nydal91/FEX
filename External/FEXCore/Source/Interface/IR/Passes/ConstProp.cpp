#if defined(_M_ARM_64)
//aarch64 heuristics
#include "aarch64/assembler-aarch64.h"
#include "aarch64/cpu-aarch64.h"
#include "aarch64/disasm-aarch64.h"
#include "aarch64/assembler-aarch64.h"
#endif

#include "Interface/IR/PassManager.h"
#include "Interface/Core/OpcodeDispatcher.h"

namespace FEXCore::IR {

class ConstProp final : public FEXCore::IR::Pass {
public:
  bool Run(IREmitter *IREmit) override;
  bool InlineConstants;
  ConstProp(bool DoInlineConstants) : InlineConstants(DoInlineConstants) { }
};

template<typename T>
static uint64_t getMask(T Op) {
  uint64_t NumBits = Op->Header.Size * 8;
  return (~0ULL) >> (64 - NumBits);
}

#ifdef _M_X86_64
// very lazy heuristics
static bool IsImmLogical(uint64_t imm, unsigned width) { return imm < 0x8000'0000; }
static bool IsImmAddSub(uint64_t imm) { return imm < 0x8000'0000; }
#elif defined(_M_ARM_64)
//aarch64 heuristics
static bool IsImmLogical(uint64_t imm, unsigned width) { if (width < 32) width = 32; return vixl::aarch64::Assembler::IsImmLogical(imm, width); }
static bool IsImmAddSub(uint64_t imm) { return vixl::aarch64::Assembler::IsImmAddSub(imm); }
#else
#error No inline constant heuristics for this target
#endif

bool ConstProp::Run(IREmitter *IREmit) {
  
  bool Changed = false;
  auto CurrentIR = IREmit->ViewIR();

  auto OriginalWriteCursor = IREmit->GetWriteCursor();

  auto HeaderOp = CurrentIR.GetHeader();
  

  for (auto [CodeNode, IROp] : CurrentIR.GetAllCode()) {

    switch (IROp->Op) {
/*
    case OP_UMUL:
    case OP_DIV:
    case OP_UDIV:
    case OP_REM:
    case OP_UREM:
    case OP_MULH:
    case OP_UMULH:
    case OP_LSHR:
    case OP_ASHR:
    case OP_ROL:
    case OP_ROR:
    case OP_LDIV:
    case OP_LUDIV:
    case OP_LREM:
    case OP_LUREM:
    case OP_BFI:
    {
      uint64_t Constant1;
      uint64_t Constant2;

      if (IREmit->IsValueConstant(IROp->Args[0], &Constant1) &&
          IREmit->IsValueConstant(IROp->Args[1], &Constant2)) {
        LogMan::Msg::A("Could const prop op: %s", std::string(IR::GetName(IROp->Op)).c_str());
      }
    break;
    }

    case OP_SEXT:
    case OP_NEG:
    case OP_POPCOUNT:
    case OP_FINDLSB:
    case OP_FINDMSB:
    case OP_REV:
    case OP_SBFE:
    {
      uint64_t Constant1;

      if (IREmit->IsValueConstant(IROp->Args[0], &Constant1)) {
        LogMan::Msg::A("Could const prop op: %s", std::string(IR::GetName(IROp->Op)).c_str());
      }
    break;
    }
*/
    case OP_ADD: {
      auto Op = IROp->C<IR::IROp_Add>();
      uint64_t Constant1;
      uint64_t Constant2;

      if (IREmit->IsValueConstant(Op->Header.Args[0], &Constant1) &&
          IREmit->IsValueConstant(Op->Header.Args[1], &Constant2)) {
        uint64_t NewConstant = (Constant1 + Constant2) & getMask(Op) ;
        IREmit->ReplaceWithConstant(CodeNode, NewConstant);
        Changed = true;
        continue;
      }
    break;
    }
    case OP_SUB: {
      auto Op = IROp->C<IR::IROp_Sub>();
      uint64_t Constant1;
      uint64_t Constant2;

      if (IREmit->IsValueConstant(Op->Header.Args[0], &Constant1) &&
          IREmit->IsValueConstant(Op->Header.Args[1], &Constant2)) {
        uint64_t NewConstant = (Constant1 - Constant2) & getMask(Op) ;
        IREmit->ReplaceWithConstant(CodeNode, NewConstant);
        Changed = true;
        continue;
      }
    break;
    }
    case OP_AND: {
      auto Op = IROp->CW<IR::IROp_And>();
      uint64_t Constant1 = 0;
      uint64_t Constant2 = 0;

      if (IREmit->IsValueConstant(Op->Header.Args[0], &Constant1) &&
          IREmit->IsValueConstant(Op->Header.Args[1], &Constant2)) {
        uint64_t NewConstant = (Constant1 & Constant2) & getMask(Op) ;
        IREmit->ReplaceWithConstant(CodeNode, NewConstant);
        Changed = true;
        continue;
      } else if (Constant2 == 1) {
        // happens from flag calcs
        auto val = IREmit->GetOpHeader(Op->Header.Args[0]);

        uint64_t Constant3;
        if (val->Op == OP_SELECT && 
            IREmit->IsValueConstant(val->Args[2], &Constant2) &&
            IREmit->IsValueConstant(val->Args[3], &Constant3) &&
            Constant2 == 1 &&
            Constant3 == 0)
        {
          IREmit->ReplaceAllUsesWith(CodeNode, CurrentIR.GetNode(Op->Header.Args[0]));
          Changed = true;
          continue;
        }
      } else if (Op->Header.Args[0].ID() == Op->Header.Args[1].ID()) {
        // AND with same value results in original value
        IREmit->ReplaceAllUsesWith(CodeNode, CurrentIR.GetNode(Op->Header.Args[0]));
        Changed = true;
        continue;
      }
    break;
    }
    case OP_OR: {
      auto Op = IROp->CW<IR::IROp_Or>();
      uint64_t Constant1;
      uint64_t Constant2;

      if (IREmit->IsValueConstant(Op->Header.Args[0], &Constant1) &&
          IREmit->IsValueConstant(Op->Header.Args[1], &Constant2)) {
        uint64_t NewConstant = Constant1 | Constant2;
        IREmit->ReplaceWithConstant(CodeNode, NewConstant);
        Changed = true;
        continue;
      } else if (Op->Header.Args[0].ID() == Op->Header.Args[1].ID()) {
        // OR with same value results in original value
        IREmit->ReplaceAllUsesWith(CodeNode, CurrentIR.GetNode(Op->Header.Args[0]));
        Changed = true;
        continue;
      }
    break;
    }
    case OP_XOR: {
      auto Op = IROp->C<IR::IROp_Xor>();
      uint64_t Constant1;
      uint64_t Constant2;

      if (IREmit->IsValueConstant(Op->Header.Args[0], &Constant1) &&
          IREmit->IsValueConstant(Op->Header.Args[1], &Constant2)) {
        uint64_t NewConstant = Constant1 ^ Constant2;
        IREmit->ReplaceWithConstant(CodeNode, NewConstant);
        Changed = true;
        continue;
      } else if (Op->Header.Args[0].ID() == Op->Header.Args[1].ID()) {
        // XOR with same value results to zero
        IREmit->SetWriteCursor(CodeNode);
        IREmit->ReplaceAllUsesWith(CodeNode, IREmit->_Constant(0));
        Changed = true;
        continue;
      }
    break;
    }
    case OP_LSHL: {
      auto Op = IROp->CW<IR::IROp_Lshl>();
      uint64_t Constant1;
      uint64_t Constant2;

      if (IREmit->IsValueConstant(Op->Header.Args[0], &Constant1) &&
          IREmit->IsValueConstant(Op->Header.Args[1], &Constant2)) {
        uint64_t NewConstant = (Constant1 << Constant2) & getMask(Op);
        IREmit->ReplaceWithConstant(CodeNode, NewConstant);
        Changed = true;
        continue;
      }
      else if (IREmit->IsValueConstant(Op->Header.Args[1], &Constant2) &&
                Constant2 == 0) {
        IREmit->SetWriteCursor(CodeNode);
        OrderedNode *Arg = CurrentIR.GetNode(Op->Header.Args[0]);
        IREmit->ReplaceAllUsesWith(CodeNode, Arg);
        Changed = true;
        continue;
      }
    break;
    }
    case OP_BFE: {
      auto Op = IROp->C<IR::IROp_Bfe>();
      uint64_t Constant;
      if (IROp->Size <= 8 && IREmit->IsValueConstant(Op->Header.Args[0], &Constant)) {
        uint64_t SourceMask = (1ULL << Op->Width) - 1;
        if (Op->Width == 64)
          SourceMask = ~0ULL;
        SourceMask <<= Op->lsb;

        uint64_t NewConstant = (Constant & SourceMask) >> Op->lsb;
        IREmit->ReplaceWithConstant(CodeNode, NewConstant);
        Changed = true;
        continue;
      } else if (IROp->Size == CurrentIR.GetOp<IROp_Header>(Op->Header.Args[0])->Size && Op->Width == (IROp->Size * 8) && Op->lsb == 0 ) {
        // A BFE that extracts all bits results in original value
  // XXX - This is broken for now - see https://github.com/FEX-Emu/FEX/issues/351
        // IREmit->ReplaceAllUsesWith(CodeNode, CurrentIR.GetNode(Op->Header.Args[0]));
        // Changed = true;
      } else if (Op->Width == 1 && Op->lsb == 0) {
        // common from flag codegen
        auto val = IREmit->GetOpHeader(Op->Header.Args[0]);

        uint64_t Constant2;
        uint64_t Constant3;
        if (val->Op == OP_SELECT && 
            IREmit->IsValueConstant(val->Args[2], &Constant2) &&
            IREmit->IsValueConstant(val->Args[3], &Constant3) &&
            Constant2 == 1 &&
            Constant3 == 0)
        {
          IREmit->ReplaceAllUsesWith(CodeNode, CurrentIR.GetNode(Op->Header.Args[0]));
          Changed = true;
          continue;
        }
      }

    break;
    }
    case OP_MUL: {
      auto Op = IROp->C<IR::IROp_Mul>();
      uint64_t Constant1;
      uint64_t Constant2;

      if (IREmit->IsValueConstant(Op->Header.Args[0], &Constant1) &&
          IREmit->IsValueConstant(Op->Header.Args[1], &Constant2)) {
        uint64_t NewConstant = (Constant1 * Constant2) & getMask(Op);
        IREmit->ReplaceWithConstant(CodeNode, NewConstant);
        Changed = true;
        continue;
      }
      break;
    }

    case OP_CONDJUMP: {
      auto Op = IROp->CW<IR::IROp_CondJump>();

      auto Select = IREmit->GetOpHeader(Op->Header.Args[0]);
      if (Select->Op == OP_SELECT) {
        
        uint64_t Constant1;
        uint64_t Constant2;

        if (IREmit->IsValueConstant(Select->Args[2], &Constant1) && IREmit->IsValueConstant(Select->Args[3], &Constant2)) {
          if (Constant1 == 1 && Constant2 == 0) {
            auto slc = Select->C<IR::IROp_Select>();
            IREmit->ReplaceNodeArgument(CodeNode, 0, IREmit->UnwarpNode(Select->Args[0]));
            IREmit->ReplaceNodeArgument(CodeNode, 1, IREmit->UnwarpNode(Select->Args[1]));
            Op->Cond = slc->Cond;
            Op->CompareSize = slc->CompareSize;
            Changed = true;
            continue;
          }
        }
      }
    }
    default: break;
    }
  }

  if (!HeaderOp->ShouldInterpret && InlineConstants) {
    for (auto [CodeNode, IROp] : CurrentIR.GetAllCode()) {
      switch(IROp->Op) {
        case OP_LSHR:
        case OP_ASHR:
        case OP_ROL:
        case OP_ROR:
        case OP_LSHL:
        {
          auto Op = IROp->C<IR::IROp_Lshr>();

          uint64_t Constant2;
          if (IREmit->IsValueConstant(Op->Header.Args[1], &Constant2)) {
            IREmit->SetWriteCursor(CurrentIR.GetNode(Op->Header.Args[1]));

            // this shouldn't be here, but rather on the emitter themselves or the constprop transformation?
            if (IROp->Size <=4)
              Constant2 &= 31;
            else
              Constant2 &= 63;

            IREmit->ReplaceNodeArgument(CodeNode, 1, IREmit->_InlineConstant(Constant2));

            Changed = true;
          }
          break;
        }

        case OP_ADD:
        case OP_SUB:
        {
          auto Op = IROp->C<IR::IROp_Add>();

          uint64_t Constant2;
          if (IREmit->IsValueConstant(Op->Header.Args[1], &Constant2)) {
            if (IsImmAddSub(Constant2)) {
              IREmit->SetWriteCursor(CurrentIR.GetNode(Op->Header.Args[1]));

              IREmit->ReplaceNodeArgument(CodeNode, 1, IREmit->_InlineConstant(Constant2));
              
              Changed = true;
            }
          }
          break;
        }

        case OP_SELECT:
        {
          auto Op = IROp->C<IR::IROp_Select>();

          uint64_t Constant1;
          if (IREmit->IsValueConstant(Op->Header.Args[1], &Constant1)) {
            if (IsImmAddSub(Constant1)) {
              IREmit->SetWriteCursor(CurrentIR.GetNode(Op->Header.Args[1]));

              IREmit->ReplaceNodeArgument(CodeNode, 1, IREmit->_InlineConstant(Constant1));
              
              Changed = true;
            }
          }

          uint64_t Constant2;
          uint64_t Constant3;
          if (IREmit->IsValueConstant(Op->Header.Args[2], &Constant2) &&
              IREmit->IsValueConstant(Op->Header.Args[3], &Constant3) &&
              Constant2 == 1 &&
              Constant3 == 0)
          {
            IREmit->SetWriteCursor(CurrentIR.GetNode(Op->Header.Args[2]));

            IREmit->ReplaceNodeArgument(CodeNode, 2, IREmit->_InlineConstant(Constant2));
            IREmit->ReplaceNodeArgument(CodeNode, 3, IREmit->_InlineConstant(Constant3));
          }

          break;
        }

        case OP_CONDJUMP:
        {
          auto Op = IROp->C<IR::IROp_CondJump>();

          uint64_t Constant2;
          if (IREmit->IsValueConstant(Op->Header.Args[1], &Constant2)) {
            if (IsImmAddSub(Constant2)) {
              IREmit->SetWriteCursor(CurrentIR.GetNode(Op->Header.Args[1]));

              IREmit->ReplaceNodeArgument(CodeNode, 1, IREmit->_InlineConstant(Constant2));
              
              Changed = true;
            }
          }
          break;
        }

        case OP_OR:
        case OP_XOR:
        case OP_AND:
        {
          auto Op = IROp->CW<IR::IROp_Or>();

          uint64_t Constant2;
          if (IREmit->IsValueConstant(Op->Header.Args[1], &Constant2)) {
            if (IsImmLogical(Constant2, IROp->Size * 8)) {
              IREmit->SetWriteCursor(CurrentIR.GetNode(Op->Header.Args[1]));

              IREmit->ReplaceNodeArgument(CodeNode, 1, IREmit->_InlineConstant(Constant2));

              Changed = true;
            }
          }
          break;
        }

        default: break;
      }
    }
  }

  IREmit->SetWriteCursor(OriginalWriteCursor);

  return Changed;
}

FEXCore::IR::Pass* CreateConstProp(bool InlineConstants) {
  return new ConstProp(InlineConstants);
}

}
