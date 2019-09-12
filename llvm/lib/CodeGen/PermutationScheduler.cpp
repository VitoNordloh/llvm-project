#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/PermutationScheduler.h"
#include "llvm/Support/Debug.h"
#include "llvm/CodeGen/Permutation.h"
#include "llvm/CodeGen/MachineBasicBlock.h"

#include <vector>
#include <algorithm>
#include <string>
#include <map>

#include "Permutations.h"

#define DEBUG_TYPE "permutation-scheduler"

using namespace std;

namespace llvm {

void PermutationScheduler::enterMBB(MachineBasicBlock *MBB) {
    this->MBB = MBB;
    regionCounter = 0;
}

void PermutationScheduler::initialize(ScheduleDAGMI *DAG) {
    dag = DAG;

    auto perm = new Permutation<unsigned int>();
    for(auto sunit : dag->SUnits) {
        perm->addInstruction(sunit.NodeNum);
        for(auto dep : sunit.Succs) {
            if(!dep.isWeak()) {
                perm->addDependency(dep.getSUnit()->NodeNum, sunit.NodeNum);
            }
        }
    }

    string name("");
    name += string(MBB->getParent()->getName()) + "_" + string(MBB->getName()) + "_" + to_string(regionCounter);

    int permutation = perms.getPermutation(MBB, regionCounter);
    LLVM_DEBUG(dbgs() << "REGION " << name << " (" << permutation << "/" << perm->countPermutations() << ")\n");
    units = perm->getPermutation(permutation);
    regionCounter++;
}

SUnit *PermutationScheduler::pickNode(bool &IsTopNode) {
    IsTopNode = true;
    SUnit *unit = nullptr;
    if(units.size() > 0) {
        for(auto sunit : queue) {
            if(sunit->NodeNum == units.front()) {
                unit = sunit;
            }
        }
        units.pop_front();
    }
    return unit;
};

void PermutationScheduler::schedNode(SUnit *SU, bool IsTopNode) {
};

void PermutationScheduler::releaseTopNode(SUnit *SU) {
    queue.push_back(SU);
};

void PermutationScheduler::releaseBottomNode(SUnit *SU) {
};
}