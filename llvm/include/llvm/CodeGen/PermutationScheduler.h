#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/ScheduleDAG.h"
#include "llvm/CodeGen/MachineBasicBlock.h"

#include <vector>
#include <list>
#include <map>

#include "Permutations.h"

#ifndef LLVM_PERMUTATIONSCHEDULER_H
#define LLVM_PERMUTATIONSCHEDULER_H

using namespace std;

namespace llvm {
class PermutationScheduler : public MachineSchedStrategy {
public:
    Permutations perms;
    ScheduleDAGMI *dag;
    vector<SUnit*> queue;
    list<unsigned int> units;
    map<string, int> permutations;
    MachineBasicBlock *MBB;
    int regionCounter;

    void enterMBB(MachineBasicBlock *MBB) override;
    void initialize(ScheduleDAGMI *DAG) override;
    SUnit *pickNode(bool &IsTopNode) override;
    void schedNode(SUnit *SU, bool IsTopNode) override;
    void releaseTopNode(SUnit *SU) override;
    void releaseBottomNode(SUnit *SU) override;
};
} // end namespace llvm


#endif //LLVM_PERMUTATIONSCHEDULER_H
