#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"

#include <map>

using namespace llvm;
using namespace std;

#ifndef LLVM_PROFILEREADER_H
#define LLVM_PROFILEREADER_H

struct ProfileReader : FunctionPass {
    static char ID;

    ProfileReader() : FunctionPass(ID) {}
    bool runOnFunction(Function &F) override;
    map<BasicBlock*, unsigned> &getBBFreq();

private:
    map<BasicBlock*, unsigned> freqMap;
};

#endif //LLVM_PROFILEREADER_H
