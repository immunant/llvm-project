//==-- AArch64DeadRegisterDefinitions.cpp - Replace dead defs w/ zero reg --==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file When allowed by the instruction, replace a dead definition of a GPR
/// with the zero register. This makes the code a bit friendlier towards the
/// hardware's register renamer.
//===----------------------------------------------------------------------===//

#include "AArch64.h"
#include "AArch64RegisterInfo.h"
#include "AArch64Subtarget.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "aarch64-dead-defs"

STATISTIC(NumDeadDefsReplaced, "Number of dead definitions replaced");

#define AARCH64_DEAD_REG_DEF_NAME "AArch64 Dead register definitions"

namespace {
class AArch64DeadRegisterDefinitions : public MachineFunctionPass {
private:
  const TargetRegisterInfo *TRI;
  const MachineRegisterInfo *MRI;
  const TargetInstrInfo *TII;
  bool Changed;
  void processMachineBasicBlock(MachineBasicBlock &MBB);
public:
  static char ID; // Pass identification, replacement for typeid.
  AArch64DeadRegisterDefinitions() : MachineFunctionPass(ID) {
    initializeAArch64DeadRegisterDefinitionsPass(
        *PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &F) override;

  StringRef getPassName() const override { return AARCH64_DEAD_REG_DEF_NAME; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};
char AArch64DeadRegisterDefinitions::ID = 0;
} // end anonymous namespace

INITIALIZE_PASS(AArch64DeadRegisterDefinitions, "aarch64-dead-defs",
                AARCH64_DEAD_REG_DEF_NAME, false, false)

static bool usesFrameIndex(const MachineInstr &MI) {
  for (const MachineOperand &MO : MI.uses())
    if (MO.isFI())
      return true;
  return false;
}

// Instructions that lose their 'read' operation for a subesquent fence acquire
// (DMB LD) once the zero register is used.
//
// WARNING: The aquire variants of the instructions are also affected, but they
// are split out into `atomicBarrierDroppedOnZero()` to support annotations on
// assembly.
static bool atomicReadDroppedOnZero(unsigned Opcode) {
  switch (Opcode) {
    case AArch64::LDADDB:     case AArch64::LDADDH:
    case AArch64::LDADDW:     case AArch64::LDADDX:
    case AArch64::LDADDLB:    case AArch64::LDADDLH:
    case AArch64::LDADDLW:    case AArch64::LDADDLX:
    case AArch64::LDCLRB:     case AArch64::LDCLRH:
    case AArch64::LDCLRW:     case AArch64::LDCLRX:
    case AArch64::LDCLRLB:    case AArch64::LDCLRLH:
    case AArch64::LDCLRLW:    case AArch64::LDCLRLX:
    case AArch64::LDEORB:     case AArch64::LDEORH:
    case AArch64::LDEORW:     case AArch64::LDEORX:
    case AArch64::LDEORLB:    case AArch64::LDEORLH:
    case AArch64::LDEORLW:    case AArch64::LDEORLX:
    case AArch64::LDSETB:     case AArch64::LDSETH:
    case AArch64::LDSETW:     case AArch64::LDSETX:
    case AArch64::LDSETLB:    case AArch64::LDSETLH:
    case AArch64::LDSETLW:    case AArch64::LDSETLX:
    case AArch64::LDSMAXB:    case AArch64::LDSMAXH:
    case AArch64::LDSMAXW:    case AArch64::LDSMAXX:
    case AArch64::LDSMAXLB:   case AArch64::LDSMAXLH:
    case AArch64::LDSMAXLW:   case AArch64::LDSMAXLX:
    case AArch64::LDSMINB:    case AArch64::LDSMINH:
    case AArch64::LDSMINW:    case AArch64::LDSMINX:
    case AArch64::LDSMINLB:   case AArch64::LDSMINLH:
    case AArch64::LDSMINLW:   case AArch64::LDSMINLX:
    case AArch64::LDUMAXB:    case AArch64::LDUMAXH:
    case AArch64::LDUMAXW:    case AArch64::LDUMAXX:
    case AArch64::LDUMAXLB:   case AArch64::LDUMAXLH:
    case AArch64::LDUMAXLW:   case AArch64::LDUMAXLX:
    case AArch64::LDUMINB:    case AArch64::LDUMINH:
    case AArch64::LDUMINW:    case AArch64::LDUMINX:
    case AArch64::LDUMINLB:   case AArch64::LDUMINLH:
    case AArch64::LDUMINLW:   case AArch64::LDUMINLX:
    return true;
  }
  return false;
}

void AArch64DeadRegisterDefinitions::processMachineBasicBlock(
    MachineBasicBlock &MBB) {
  const MachineFunction &MF = *MBB.getParent();
  for (MachineBasicBlock::iterator II = MBB.begin(), E = MBB.end(); II != E; ++II) {
    MachineInstr &MI = *II;
    if (MI.mayLoadOrStore()) {
      bool tag = true;
      unsigned operand_number = 0;
      /*
       * TODO: check if we need to do anything for LDRDl, LDRQl, LDRSWl, LDRSl,
       * LDRWl and LDRXl. They load literals and have no GPR64sp operands but
       * are marked as may load
       */
      switch (MI.getOpcode()) {
        case AArch64::LDRBBroW:
        case AArch64::LDRBBroX:
        case AArch64::LDRBBui:
        case AArch64::LDRBroW:
        case AArch64::LDRBroX:
        case AArch64::LDRBui:
        case AArch64::LDRDroW:
        case AArch64::LDRDroX:
        case AArch64::LDRDui:
        case AArch64::LDRHHroW:
        case AArch64::LDRHHroX:
        case AArch64::LDRHHui:
        case AArch64::LDRHroW:
        case AArch64::LDRHroX:
        case AArch64::LDRHui:
        case AArch64::LDRQroW:
        case AArch64::LDRQroX:
        case AArch64::LDRQui:
        case AArch64::LDRSBWroW:
        case AArch64::LDRSBWroX:
        case AArch64::LDRSBWui:
        case AArch64::LDRSBXroW:
        case AArch64::LDRSBXroX:
        case AArch64::LDRSBXui:
        case AArch64::LDRSHWroW:
        case AArch64::LDRSHWroX:
        case AArch64::LDRSHWui:
        case AArch64::LDRSHXroW:
        case AArch64::LDRSHXroX:
        case AArch64::LDRSHXui:
        case AArch64::LDRSWroW:
        case AArch64::LDRSWroX:
        case AArch64::LDRSWui:
        case AArch64::LDRSroW:
        case AArch64::LDRSroX:
        case AArch64::LDRSui:
        case AArch64::LDRWroW:
        case AArch64::LDRWroX:
        case AArch64::LDRWui:
        case AArch64::LDRXroW:
        case AArch64::LDRXroX:
        case AArch64::LDRXui:
        case AArch64::LDR_PXI:
        case AArch64::LDR_TX:
        case AArch64::LDR_ZXI:
        case AArch64::LDRAAindexed:
        case AArch64::LDRABindexed:
        case AArch64::STRBBroW:
        case AArch64::STRBBroX:
        case AArch64::STRBBui:
        case AArch64::STRBroW:
        case AArch64::STRBroX:
        case AArch64::STRBui:
        case AArch64::STRDroW:
        case AArch64::STRDroX:
        case AArch64::STRDui:
        case AArch64::STRHHroW:
        case AArch64::STRHHroX:
        case AArch64::STRHHui:
        case AArch64::STRHroW:
        case AArch64::STRHroX:
        case AArch64::STRHui:
        case AArch64::STRQroW:
        case AArch64::STRQroX:
        case AArch64::STRQui:
        case AArch64::STRSroW:
        case AArch64::STRSroX:
        case AArch64::STRSui:
        case AArch64::STRWroW:
        case AArch64::STRWroX:
        case AArch64::STRWui:
        case AArch64::STRXroW:
        case AArch64::STRXroX:
        case AArch64::STRXui:
        case AArch64::STR_PXI:
        case AArch64::STR_TX:
        case AArch64::STR_ZXI:
            operand_number = 1;
            break;
        case AArch64::LDPDi:
        case AArch64::LDPQi:
        case AArch64::LDPSWi:
        case AArch64::LDPSi:
        case AArch64::LDPWi:
        case AArch64::LDPXi:
        case AArch64::LDRAAwriteback:
        case AArch64::LDRABwriteback:
        case AArch64::LDRBBpost:
        case AArch64::LDRBBpre:
        case AArch64::LDRBpost:
        case AArch64::LDRBpre:
        case AArch64::LDRDpost:
        case AArch64::LDRDpre:
        case AArch64::LDRHHpost:
        case AArch64::LDRHHpre:
        case AArch64::LDRHpost:
        case AArch64::LDRHpre:
        case AArch64::LDRQpost:
        case AArch64::LDRQpre:
        case AArch64::LDRSBWpost:
        case AArch64::LDRSBWpre:
        case AArch64::LDRSBXpost:
        case AArch64::LDRSBXpre:
        case AArch64::LDRSHWpost:
        case AArch64::LDRSHWpre:
        case AArch64::LDRSHXpost:
        case AArch64::LDRSHXpre:
        case AArch64::LDRSWpost:
        case AArch64::LDRSWpre:
        case AArch64::LDRSpost:
        case AArch64::LDRSpre:
        case AArch64::LDRWpost:
        case AArch64::LDRWpre:
        case AArch64::LDRXpost:
        case AArch64::LDRXpre:
        case AArch64::STPDi:
        case AArch64::STPQi:
        case AArch64::STPSi:
        case AArch64::STPWi:
        case AArch64::STPXi:
        case AArch64::STRBBpost:
        case AArch64::STRBBpre:
        case AArch64::STRBpost:
        case AArch64::STRBpre:
        case AArch64::STRDpost:
        case AArch64::STRDpre:
        case AArch64::STRHHpost:
        case AArch64::STRHHpre:
        case AArch64::STRHpost:
        case AArch64::STRHpre:
        case AArch64::STRQpost:
        case AArch64::STRQpre:
        case AArch64::STRSpost:
        case AArch64::STRSpre:
        case AArch64::STRWpost:
        case AArch64::STRWpre:
        case AArch64::STRXpost:
        case AArch64::STRXpre:
            operand_number = 2;
            break;
        case AArch64::LDPDpost:
        case AArch64::LDPDpre:
        case AArch64::LDPQpost:
        case AArch64::LDPQpre:
        case AArch64::LDPSWpost:
        case AArch64::LDPSWpre:
        case AArch64::LDPSpost:
        case AArch64::LDPSpre:
        case AArch64::LDPWpost:
        case AArch64::LDPWpre:
        case AArch64::LDPXpost:
        case AArch64::LDPXpre:
        case AArch64::LDR_ZA:
        case AArch64::STPDpost:
        case AArch64::STPDpre:
        case AArch64::STPQpost:
        case AArch64::STPQpre:
        case AArch64::STPSpost:
        case AArch64::STPSpre:
        case AArch64::STPWpost:
        case AArch64::STPWpre:
        case AArch64::STPXpost:
        case AArch64::STPXpre:
        case AArch64::STR_ZA:
            operand_number = 3;
            break;
        default:
            tag = false;
            break;
      }
      assert(tag);
      const MCInstrDesc &EXTRII = TII->get(AArch64::EXTRWrri);

      auto op = MI.getOperand(operand_number);
      LLVM_DEBUG(dbgs() << "opcode num is " << MI.getOpcode() << " checking operand " << operand_number << "\n");
      assert(op.isReg());
      auto reg = op.getReg();
      MachineInstrBuilder MIB2 = BuildMI(MBB, MI, MI.getDebugLoc(), EXTRII);
      MIB2.addReg(reg).addReg(reg).addReg(AArch64::X18).addImm(56);

      MachineInstrBuilder MIB3 = BuildMI(MBB, MI, MI.getDebugLoc(), EXTRII);
      MIB3.addReg(reg).addReg(reg).addReg(reg).addImm(4);
      II = MIB3;
      II++;
      Changed = true;
      continue;
    }
    if (usesFrameIndex(MI)) {
      // We need to skip this instruction because while it appears to have a
      // dead def it uses a frame index which might expand into a multi
      // instruction sequence during EPI.
      LLVM_DEBUG(dbgs() << "    Ignoring, operand is frame index\n");
      continue;
    }
    if (MI.definesRegister(AArch64::XZR) || MI.definesRegister(AArch64::WZR)) {
      // It is not allowed to write to the same register (not even the zero
      // register) twice in a single instruction.
      LLVM_DEBUG(
          dbgs()
          << "    Ignoring, XZR or WZR already used by the instruction\n");
      continue;
    }

    if (atomicBarrierDroppedOnZero(MI.getOpcode()) || atomicReadDroppedOnZero(MI.getOpcode())) {
      LLVM_DEBUG(dbgs() << "    Ignoring, semantics change with xzr/wzr.\n");
      continue;
    }

    const MCInstrDesc &Desc = MI.getDesc();
    for (int I = 0, E = Desc.getNumDefs(); I != E; ++I) {
      MachineOperand &MO = MI.getOperand(I);
      if (!MO.isReg() || !MO.isDef())
        continue;
      // We should not have any relevant physreg defs that are replacable by
      // zero before register allocation. So we just check for dead vreg defs.
      Register Reg = MO.getReg();
      if (!Reg.isVirtual() || (!MO.isDead() && !MRI->use_nodbg_empty(Reg)))
        continue;
      assert(!MO.isImplicit() && "Unexpected implicit def!");
      LLVM_DEBUG(dbgs() << "  Dead def operand #" << I << " in:\n    ";
                 MI.print(dbgs()));
      // Be careful not to change the register if it's a tied operand.
      if (MI.isRegTiedToUseOperand(I)) {
        LLVM_DEBUG(dbgs() << "    Ignoring, def is tied operand.\n");
        continue;
      }
      const TargetRegisterClass *RC = TII->getRegClass(Desc, I, TRI, MF);
      unsigned NewReg;
      if (RC == nullptr) {
        LLVM_DEBUG(dbgs() << "    Ignoring, register is not a GPR.\n");
        continue;
      } else if (RC->contains(AArch64::WZR))
        NewReg = AArch64::WZR;
      else if (RC->contains(AArch64::XZR))
        NewReg = AArch64::XZR;
      else {
        LLVM_DEBUG(dbgs() << "    Ignoring, register is not a GPR.\n");
        continue;
      }
      LLVM_DEBUG(dbgs() << "    Replacing with zero register. New:\n      ");
      MO.setReg(NewReg);
      MO.setIsDead();
      LLVM_DEBUG(MI.print(dbgs()));
      ++NumDeadDefsReplaced;
      Changed = true;
      // Only replace one dead register, see check for zero register above.
      break;
    }
  }
}

// Scan the function for instructions that have a dead definition of a
// register. Replace that register with the zero register when possible.
bool AArch64DeadRegisterDefinitions::runOnMachineFunction(MachineFunction &MF) {
  if (skipFunction(MF.getFunction()))
    return false;

  TRI = MF.getSubtarget().getRegisterInfo();
  TII = MF.getSubtarget().getInstrInfo();
  MRI = &MF.getRegInfo();
  LLVM_DEBUG(dbgs() << "***** AArch64DeadRegisterDefinitions *****\n");
  Changed = false;
  for (auto &MBB : MF)
    processMachineBasicBlock(MBB);
  return Changed;
}

FunctionPass *llvm::createAArch64DeadRegisterDefinitions() {
  return new AArch64DeadRegisterDefinitions();
}
