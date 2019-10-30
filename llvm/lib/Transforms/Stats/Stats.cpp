#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/Statistic.h"

#include <fstream>
#include <map>

#include "llvm/Transforms/Stats/Stats.h"

using namespace llvm;
using namespace std;

#define DEBUG_TYPE "profile-reader"

STATISTIC(NumFunctions, "Number of functions");
STATISTIC(NumBasicBlocks, "Number of basic blocks");
STATISTIC(LargestBasicBlock, "Largest basic block");

bool Stats::runOnFunction(Function &F) {
    ++NumFunctions;

    for(auto &BB : F) {
        ++NumBasicBlocks;
        if(BB.size() > LargestBasicBlock) {
            LargestBasicBlock = BB.size();
            dbgs() << F.getName() << "_" << BB.getName() << " (" << BB.size() << ")\n";
        }
    }

    return false;
}

char Stats::ID = 0;
static RegisterPass<Stats> X("bb-stats", "Shows statistics", true, true);

