//===-- AArch64LoadStoreTagging.h - Tag addrs used in loads and stores ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_AARCH64LOADSTORETAGGING_H
#define LLVM_TRANSFORMS_AARCH64LOADSTORETAGGING_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class AArch64LoadStoreTaggingPass : public PassInfoMixin<AArch64LoadStoreTaggingPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);

  static bool isRequired() { return true; }
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_AARCH64LOADSTORETAGGING_H
