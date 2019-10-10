#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/PassAnalysisSupport.h"
#include "llvm/Support/Debug.h"

#include <vector>

using namespace llvm;
using namespace std;

#define DEBUG_TYPE "superblock-scheduler"

namespace {
    class SNode {
        Instruction *inst;
    };

    struct SuperblockScheduler : FunctionPass {
        static char ID;

        SuperblockScheduler() : FunctionPass(ID) {}

        bool runOnFunction(Function &F) override;

        void getAnalysisUsage(AnalysisUsage &AU) const override;

        vector<BasicBlock*> getSuperblock(BasicBlock *start);
    };

    bool SuperblockScheduler::runOnFunction(Function &F) {
        DependenceInfo &LI = getAnalysis<DependenceAnalysisWrapperPass>().getDI();

        vector<BasicBlock*> SB = getSuperblock(&F.front());

        dbgs() << "Superblock: ";
        for(auto &BB : SB) {
            dbgs() << BB->getName() << " - ";
        }
        dbgs() << "\n";

        return false;
    }

    void SuperblockScheduler::getAnalysisUsage(AnalysisUsage &AU) const {
        AU.setPreservesCFG();
        AU.addRequired<DependenceAnalysisWrapperPass>();
    }

    vector<BasicBlock*> getSuperblock(BasicBlock *start) {
        vector<BasicBlock*> MBBS(0);
        MBBS.push_back(start);

        BasicBlock* curr = start;
        while(true) {
            // The block is only allowed to have one predecessor
            if(pred_size(curr) > 1) {
                break;
            }

            // Add the current MBB to the superblock
            MBBS.push_back(curr);

            // We need a successor
            if(succ_size(curr) < 1) {
                break;
            }

            // Update curr
            curr = *(successors(curr).begin());
        };
        return MBBS;
    }
}

char SuperblockScheduler::ID = 0;
static RegisterPass<SuperblockScheduler> X("superblock-scheduler", "Superblock Scheduler.");