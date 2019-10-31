#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"

#include "llvm/Transforms/ProfileReader/ProfileReader.h"

#include <fstream>
#include <map>

using namespace llvm;
using namespace std;

#define DEBUG_TYPE "profile-reader"

static cl::opt<string> ProfileName("profile-name", cl::desc("Filename of the profile."));

namespace {
    vector<string> *split(string *s, string const &delim) {
        auto *parts = new vector<string>();
        size_t current = s->find(delim);
        size_t previous = 0;
        while(current != string::npos) {
            parts->push_back(s->substr(previous, current - previous));
            previous = current + 1;
            current = s->find(delim, previous);
        }
        parts->push_back(s->substr(previous, current - previous));
        return parts;
    }
}

void ProfileReader::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<BlockFrequencyInfoWrapperPass>();
    AU.addRequired<BranchProbabilityInfoWrapperPass>();
}

bool ProfileReader::runOnFunction(Function &F) {
    auto BFI = &getAnalysis<BlockFrequencyInfoWrapperPass>().getBFI();
    auto BPI = &getAnalysis<BranchProbabilityInfoWrapperPass>().getBPI();

    freqMap.clear();
    std::ifstream file(ProfileName);
    string line;

    // Skip header
    getline(file, line);

    while(getline(file, line)) {
        vector<string> *splitted = split(&line, ";");
        if(splitted->size() != 2) {
            break;
        }
        string BBName = splitted->at(0);
        int freq = (int) stol(splitted->at(1));

        // Search for BB
        for(auto &BB : F) {
            string thisName = F.getName().str() + "_" + BB.getName().str();
            if(thisName == BBName) {
                freqMap.insert(pair<BasicBlock*,unsigned>(&BB, freq));
                BFI->setBlockFreq(&BB, freq);
                BPI->setEdgeProbability(&BB, 0, BranchProbability(999, 999));
            }
        }
    }

    BFI->print(dbgs());
    return false;
}

map<BasicBlock*,unsigned> &ProfileReader::getBBFreq() {
    return freqMap;
}

char ProfileReader::ID = 0;
static RegisterPass<ProfileReader> X("profile-reader", "Profile Reader", true, true);

