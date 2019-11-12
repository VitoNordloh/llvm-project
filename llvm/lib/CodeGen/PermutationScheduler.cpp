#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/Permutations.h"
#include "llvm/CodeGen/PermutationScheduler.h"
#include "llvm/Permutation/Permutation.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <stdlib.h>
#include <string>
#include <sys/time.h>
#include <time.h>

#define DEBUG_TYPE "permutation-scheduler"

using namespace std;

namespace llvm {

static ScheduleDAGMI *currDag;

string labelFn(unsigned int i) {
    string result = "(" + to_string(i) + ") ";
    for(auto sunit : currDag->SUnits) {
        if(sunit.NodeNum == i) {
            string s;
            auto *o = new raw_string_ostream(s);
            // sunit.print(*o);
            sunit.getInstr()->print(*o);
            result += s;
            delete o;
        }
    }
    // return result;
    return to_string(i);
}

void PermutationScheduler::enterMBB(MachineBasicBlock *MBB) {
    this->MBB = MBB;
    regionCounter = 0;
}

void PermutationScheduler::initialize(ScheduleDAGMI *DAG) {
    dag = DAG;
    currDag = DAG;

    for(auto sunit : dag->SUnits) {
        dbgs() << "(" << sunit.NodeNum << ") ";
        DAG->dumpNode(&sunit);
    }

    auto perm = new Permutation<unsigned int>();
    for(auto sunit : dag->SUnits) {
        perm->addInstruction(sunit.NodeNum);
        for(auto dep : sunit.Succs) {
            if(!dep.isArtificial() && !dep.isCluster()) {
                perm->addDependency(dep.getSUnit()->NodeNum, sunit.NodeNum);
            }
        }
    }

    perm->setLabelCallback(labelFn);
    perm->dumpDot("dep.dot", {});

    string name;
    name += string(MBB->getParent()->getName()) + "_" + string(MBB->getName()) + "_" + to_string(regionCounter);

    /*int permutation = perms.getPermutation(MBB, regionCounter);
    LLVM_DEBUG(dbgs() << "REGION " << name << " (" << permutation << "/" << perm->countPermutations() << ")\n");
    units = perm->getPermutation(permutation)->toList();*/

    // Get random permutation
    timeval t;
    gettimeofday(&t, nullptr);
    srand(t.tv_sec + t.tv_usec);
    units = perm->getRandomPermutation();

    // Print random permutation
    dbgs() << "SCHEDULE " << name << ":";
    for(auto &i : units) {
        dbgs() << i << ",";
    }
    dbgs() << "\n";

    regionCounter++;
}

SUnit *PermutationScheduler::pickNode(bool &IsTopNode) {
    IsTopNode = true;
    SUnit *unit = nullptr;
    if(!units.empty()) {
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
    auto start = find(queue.begin(), queue.end(), SU);
    queue.erase(start);
};

void PermutationScheduler::releaseTopNode(SUnit *SU) {
    queue.push_back(SU);
    dbgs() << "Released top node: " << queue.size() << " nodes in queue\n";
};

void PermutationScheduler::releaseBottomNode(SUnit *SU) {
};
}