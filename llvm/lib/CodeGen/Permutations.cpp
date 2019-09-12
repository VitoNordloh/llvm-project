#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/Support/CommandLine.h"

#include <string>
#include <vector>

#include "Permutations.h"

namespace llvm {
    vector<string>* split(string* s, string delim) {
        vector<string>* parts = new vector<string>();
        size_t current = s->find(delim);
        size_t previous = 0;
        while(current != string::npos) {
            parts->push_back(s->substr(previous, current-previous));
            previous = current + 1;
            current = s->find(delim, previous);
        }
        parts->push_back(s->substr(previous, current-previous));
        return parts;
    }

    cl::opt<string> permsMbb("perms-mbb");
    cl::opt<string> perms("perms");

    Permutations::Permutations() {
        // Get enabled MBB
        enabledMBB = split(&permsMbb, ",");

        // Get permutations for regions
        if(perms.getNumOccurrences()) {
            vector <string> *mbbPerms = split(&perms, ",");
            for (auto mbb : *mbbPerms) {
                vector <string> *permString = split(&mbb, "=");
                string regionName = permString->at(0);
                int perm = stoi(permString->at(1));
                permutations.insert(pair<string, int>(regionName, perm));
            }
        }
    }

    int Permutations::getPermutation(MachineBasicBlock *MBB, int region) {
        string name("");
        name += string(MBB->getParent()->getName()) + "_" + string(MBB->getName()) + "_" + to_string(region);
        if(permutations.find(name) == permutations.end()) return 0;
        return permutations.at(name);
    }

    bool Permutations::isEnabled(MachineBasicBlock *MBB) {
        string name("");
        name += string(MBB->getParent()->getName()) + "_" + string(MBB->getName());
        return find(enabledMBB->begin(), enabledMBB->end(), name) != enabledMBB->end();
    }
}