#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"

using namespace llvm;
using namespace std;

#ifndef LLVM_SUPERBLOCKFINDER_H
#define LLVM_SUPERBLOCKFINDER_H

struct SuperblockFinder : FunctionPass {
    static char ID;

    SuperblockFinder() : FunctionPass(ID) {}
    bool runOnFunction(Function &F) override;
    void getAnalysisUsage(AnalysisUsage &AU) const override;
    vector<BasicBlock*> &getSB();

private:
    vector<BasicBlock*> SB;
};

#endif //LLVM_SUPERBLOCKFINDER_H
