#include "llvm/IR/Function.h"
#include "llvm/Pass.h"

using namespace llvm;
using namespace std;

#ifndef LLVM_STATS_H
#define LLVM_STATS_H

struct Stats : FunctionPass {
    static char ID;

    Stats() : FunctionPass(ID) {}
    bool runOnFunction(Function &F) override;
};

#endif //LLVM_STATS_H
