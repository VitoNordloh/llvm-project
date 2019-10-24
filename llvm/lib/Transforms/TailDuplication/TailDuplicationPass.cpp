#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/PassAnalysisSupport.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/SuperblockFinder/SuperblockFinder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"

#include <list>
#include <vector>

using namespace llvm;
using namespace std;

#define DEBUG_TYPE "tail-duplication"

namespace {
    struct TailDuplication : FunctionPass {
        static char ID;

        TailDuplication() : FunctionPass(ID) {}
        bool runOnFunction(Function &F) override;
        void getAnalysisUsage(AnalysisUsage &AU) const override;

    private:
        BasicBlock* findBasicBlock(string name);
        void duplicate(vector<BasicBlock*>::iterator it);

        Function *F;
        vector<BasicBlock*> *trace;
    };

    bool TailDuplication::runOnFunction(Function &F) {
        if(!F.getName().equals("main")) {
            return false;
        }
        this->F = &F;

        // Get trace from the SuperblockFinder
        trace = &getAnalysis<SuperblockFinder>().getSB();

        for(auto BB = next(trace->begin()); BB != trace->end(); ++BB) {
            if(pred_size(*BB) <= 1) {
                continue;
            }
            dbgs() << "BB '" << (*BB)->getName() << "' has more than 1 pred\n";
            duplicate(BB);
        }

        F.print(dbgs());

        return true;
    }

    void TailDuplication::getAnalysisUsage(AnalysisUsage &AU) const {
        AU.setPreservesCFG();
        AU.addRequired<SuperblockFinder>();
    }

    BasicBlock* TailDuplication::findBasicBlock(string name) {
        for(auto &BB : *F) {
            if(BB.getName().equals(name)) {
                return &BB;
            }
        }
        return nullptr;
    }

    void TailDuplication::duplicate(vector<BasicBlock*>::iterator it) {
        BasicBlock *BB = *it;

        // Clone BasicBlock
        ValueToValueMapTy map;
        BasicBlock *clone = CloneBasicBlock(BB, map);
        clone->setName(BB->getName()+"_clone");

        // Insert clone into function
        clone->insertInto(F, BB);

        // Fix the successors of the terminators after cloning the block
        list<BasicBlock*> toChange;
        for(auto pred : predecessors(BB)) {
            if(pred != *prev(it)) {
                dbgs() << "  BB '" << pred->getName() << "' must be fixed\n";
                toChange.push_back(pred);
            } else {
                dbgs() << "  BB '" << pred->getName() << "' must not be fixed\n";
            }
        }
        for(auto pred : toChange) {
            for(unsigned i = 0; i < pred->getTerminator()->getNumSuccessors(); i++) {
                BasicBlock *succ = pred->getTerminator()->getSuccessor(i);
                if(succ == BB) {
                    pred->getTerminator()->setSuccessor(i, clone);
                }
            }
        }

        // The BasicBlock should only have one predecessor
        assert(pred_size(BB) == 1 && "BasicBlock should have only one predecessor");

        // Add additional PHIs
        for(auto succ : successors(clone)) {
            for(auto &phi : succ->phis()) {
                for(unsigned i = 0; i < phi.getNumIncomingValues(); i++) {
                    BasicBlock *incomingBlock = phi.getIncomingBlock(i);
                    if(incomingBlock == BB) {
                        phi.addIncoming(map[phi.getIncomingValue(i)], clone);
                    }
                }
            }
        }

        // Update PHI nodes of the BasicBlock which belongs to the trace
        for(auto &phi : BB->phis()) {
            for(signed i = phi.getNumIncomingValues()-1; i >= 0; i--) {
                BasicBlock *incomingBlock = phi.getIncomingBlock(i);
                if(incomingBlock != *pred_begin(BB)) {
                    phi.removeIncomingValue(i, true);
                }
            }
        }

        // Update PHI nodes of the BasicBlock which was cloned
        for(auto &phi : clone->phis()) {
            for(signed i = phi.getNumIncomingValues()-1; i >= 0; i--) {
                BasicBlock *incomingBlock = phi.getIncomingBlock(i);
                if(incomingBlock == *pred_begin(BB)) {
                    phi.removeIncomingValue(i, true);
                }
            }
        }

        // Update SSA form
        list<Instruction*> toRemove;
        for(auto &I : *BB) {
            // Void types do not have to be corrected
            if(I.getType()->isVoidTy()) {
                continue;
            }

            // Copy the instruction (makes SSA updating a bit easier)
            Instruction *IClone = I.clone();
            IClone->insertBefore(&I);

            // Create SSAUpdater
            SSAUpdater ssa;
            ssa.Initialize(I.getType(), I.getName());
            ssa.AddAvailableValue(BB, IClone);
            ssa.AddAvailableValue(clone, map[&I]);

            // Update uses
            while(!I.use_empty()) {
                ssa.RewriteUseAfterInsertions(*I.use_begin());
            }

            // Remove original instruction
            toRemove.push_back(&I);
        }

        // Remove original instructions
        for(auto I : toRemove) {
            I->eraseFromParent();
        }
    }
}

char TailDuplication::ID = 0;
static RegisterPass<TailDuplication> X("tail-duplication", "Tail Duplication.");
