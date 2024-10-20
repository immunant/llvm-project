//===---------------------- EntryStage.h ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
///
/// This file defines the Entry stage of an instruction pipeline.  Its sole
/// purpose in life is to pick instructions in sequence and move them to the
/// next pipeline stage.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_MCA_STAGES_ENTRYSTAGE_H
#define LLVM_MCA_STAGES_ENTRYSTAGE_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/MCA/SourceMgr.h"
#include "llvm/MCA/Stages/Stage.h"

namespace llvm {
namespace mca {
class MetadataRegistry;

class EntryStage final : public Stage {
  InstRef CurrentInstruction;
  SmallVector<std::unique_ptr<Instruction>, 16> Instructions;
  SourceMgrBase &SM;
  unsigned NumRetired;
  MetadataRegistry *MDRegistry;

  // Updates the program counter, and sets 'CurrentInstruction'.
  Error getNextInstruction();

  EntryStage(const EntryStage &Other) = delete;
  EntryStage &operator=(const EntryStage &Other) = delete;

public:
  EntryStage(SourceMgrBase &SM, MetadataRegistry *MDR = nullptr)
    : CurrentInstruction(), SM(SM), NumRetired(0), MDRegistry(MDR) { }

  bool isAvailable(const InstRef &IR) const override;
  bool hasWorkToComplete() const override;
  Error execute(InstRef &IR) override;
  Error cycleStart() override;
  Error cycleResume() override;
  Error cycleEnd() override;
};

} // namespace mca
} // namespace llvm

#endif // LLVM_MCA_STAGES_ENTRYSTAGE_H
