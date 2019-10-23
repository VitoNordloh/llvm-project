#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/ProfileReader/ProfileReader.h"
#include "llvm/Transforms/SuperblockFinder/SuperblockFinder.h"

#include <fstream>
#include <map>
#include <string>

using namespace llvm;
using namespace std;

#define DEBUG_TYPE "superblock-finder"

static cl::opt<string> SuperblockStart("superblock-start", cl::desc("Force the start block of the superblock."));

bool SuperblockFinder::runOnFunction(Function &F) {
    if(!F.getName().equals("dijkstra")) {
        return false;
    }

    SB.clear();

    this->F = &F;

    SB.push_back(findBasicBlock("for.body14"));
    SB.push_back(findBasicBlock("if.then20"));
    SB.push_back(findBasicBlock("for.inc39"));

    dbgs() << "Using const Superblock:\n";
    for(auto &BB : SB) {
        dbgs() << "  " << BB->getName() << "\n";
    }

    return false;

    // Get BasicBlock Frequency
    map<BasicBlock*, unsigned> *freqMap = &getAnalysis<ProfileReader>().getBBFreq();

    // Find most important BasicBlock
    BasicBlock *start = nullptr;
    unsigned freq = 0;
    for(auto &BB : F) {
        if(SuperblockStart.empty()) {
            if (freqMap->find(&BB) == freqMap->end()) {
                continue;
            }
            unsigned thisFreq = freqMap->at(&BB);
            if (thisFreq > freq) {
                freq = thisFreq;
                start = &BB;
            }
        } else if(SuperblockStart == BB.getName().str()) {
            start = &BB;
        }
    }

    assert(start != nullptr && "No Start BasicBlock found");

    BasicBlock* curr = start;
    bool pushBack = true;
    while(SB.size() < 6) {
        // If curr is already part of the SB, exit
        if(find(SB.begin(), SB.end(), curr) != SB.end()) {
            break;
        }

        // Add the current MBB to the superblock
        if(pushBack) {
            SB.push_back(curr);
        } else {
            SB.insert(SB.begin(), curr);
        }

        // We need a successor
        if(succ_size(curr) < 1) {
            break;
        }

        BasicBlock *next = nullptr;
        freq = 0;

        // Iterate over all successors
        for(auto BB : successors(curr)) {
            if(find(SB.begin(), SB.end(), BB) != SB.end()) {
                continue;
            }

            if(freqMap->find(BB) == freqMap->end()) {
                continue;
            }

            unsigned thisFreq = freqMap->at(BB);
            if(thisFreq > freq) {
                freq = thisFreq;
                next = BB;
                pushBack = true;
            }
        }

        // Iterate over all predecessors
        for(auto BB : predecessors(curr)) {
            if(find(SB.begin(), SB.end(), BB) != SB.end()) {
                continue;
            }

            if(freqMap->find(BB) == freqMap->end()) {
                continue;
            }

            unsigned thisFreq = freqMap->at(BB);
            if(thisFreq > freq) {
                freq = thisFreq;
                next = BB;
                pushBack = false;
            }
        }

        if(next == nullptr) {
            break;
        }
        curr = next;
    };

    // Print trace
    dbgs() << "Found Superblock:\n";
    for(auto &BB : SB) {
        dbgs() << "  " << BB->getName() << " (" << freqMap->at(BB) << ")\n";
    }

    return false;
}

void SuperblockFinder::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesCFG();
    AU.addRequired<ProfileReader>();
}

vector<BasicBlock*> &SuperblockFinder::getSB() {
    return SB;
}

BasicBlock *SuperblockFinder::findBasicBlock(const string &name) {
    for(auto &BB : *F) {
        if(BB.getName().equals(name)) {
            return &BB;
        }
    }
}

char SuperblockFinder::ID = 0;
static RegisterPass<SuperblockFinder> X("superblock-finder", "SuperBlock Finder", true, true);

