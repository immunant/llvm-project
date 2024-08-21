//===-- AArch64LoadStoreTagging.cpp - Tag addrs used in loads and stores --===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/AArch64LoadStoreTagging.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"

using namespace llvm;

Value *readRegister(IRBuilder<> &IRB, StringRef Name) {
  auto *C = &IRB.getContext();
  Module *M = IRB.GetInsertBlock()->getParent()->getParent();
  auto &DL = M->getDataLayout();
  Type *IntptrTy = IRB.getIntPtrTy(DL);

  Function *ReadRegister =
      Intrinsic::getDeclaration(M, Intrinsic::read_register, IntptrTy);
  MDNode *MD = MDNode::get(*C, {MDString::get(*C, Name)});
  Value *Args[] = {MetadataAsValue::get(*C, MD)};
  return IRB.CreateCall(ReadRegister, Args);
}

PreservedAnalyses
AArch64LoadStoreTaggingPass::run(Function &F, FunctionAnalysisManager &AM) {
  errs() << "instrumenting AArch64 loads/stores in " << F.getName() << "\n";

  IRBuilder<> IRB(F.getContext());
  Module *M = F.getParent();
  auto &DL = M->getDataLayout();
  Type *IntptrTy = IRB.getIntPtrTy(DL);
  Type *PtrTy = IRB.getPtrTy();

  auto *FiftySix = ConstantInt::get(IntptrTy, 56);

  for (Function::iterator BB = F.begin(), BBE = F.end(); BB != BBE; ++BB) {
    BasicBlock &B = *BB;
    for (BasicBlock::iterator I = B.begin(), IE = B.end(); I != IE; ++I) {
      StoreInst *SI = dyn_cast<StoreInst>(&*I);
      LoadInst *LI = dyn_cast<LoadInst>(&*I);
      if (LI || SI) {
        IRBuilder<> IRB(&*I);
        Value *ReadX18 = readRegister(IRB, "x18");
        Value *Pointer = SI ? SI->getPointerOperand() : LI->getPointerOperand();
        Value *PtrToInt = IRB.CreatePtrToInt(Pointer, IntptrTy, "makeint");

        Value *Shl = IRB.CreateShl(ReadX18, FiftySix, "shift56");
        Value *Or = IRB.CreateOr(PtrToInt, Shl, "ortag");

        Value *IntToPtrInst = IRB.CreateIntToPtr(Or, PtrTy, "makeptr");
        if (SI) {
          SI->setOperand(SI->getPointerOperandIndex(), IntToPtrInst);
        } else {
          assert(LI);
          LI->setOperand(LI->getPointerOperandIndex(), IntToPtrInst);
        }
      }
    }
  }
  // return true;

  return PreservedAnalyses::all();
}
