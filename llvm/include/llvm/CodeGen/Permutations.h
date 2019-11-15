#include "llvm/CodeGen/MachineBasicBlock.h"

#include <map>
#include <list>
#include <string>
#include <vector>

#ifndef LLVM_PERMUTATIONS_H
#define LLVM_PERMUTATIONS_H

using namespace std;

namespace llvm {
    class Permutations {
    public:
        Permutations();
        int getPermutation(MachineBasicBlock *MBB, int region);
        list<unsigned int> getSchedule();
        bool hasSchedule(MachineBasicBlock *MBB);
        bool isEnabled(MachineBasicBlock *MBB);
    private:
        vector<string> *enabledMBB;
        map<string, int> permutations;
        string mbbToSchedule;
        list<unsigned int> schedule;
    };
}


#endif //LLVM_PERMUTATIONS_H