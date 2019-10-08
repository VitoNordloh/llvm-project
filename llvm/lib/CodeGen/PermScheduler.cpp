#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/ScheduleDAG.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"

#include <iostream>

using namespace llvm;
using namespace std;

#define DEBUG_TYPE "perm-scheduler"

namespace {
    class PermScheduler : public MachineFunctionPass {
    public:
        PermScheduler() : MachineFunctionPass(ID) {
            initializePermSchedulerPass(*PassRegistry::getPassRegistry());
        }

        bool runOnMachineFunction(MachineFunction&) override;
        static char ID; // Class identification, replacement for typeinfo
    };

    bool PermScheduler::runOnMachineFunction(MachineFunction &MF) {
        LLVM_DEBUG(dbgs() << "Running on Machine Function: " << MF.getName() << "\n");
        return false;
    }
}

char PermScheduler::ID = 0;
char &llvm::PermSchedulerID = PermScheduler::ID;

INITIALIZE_PASS(PermScheduler, DEBUG_TYPE, "PermutationScheduler", false, false)