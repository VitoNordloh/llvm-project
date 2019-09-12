#include "llvm/Pass.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/APInt.h"
#include "llvm/Support/CommandLine.h"

#include <string>
#include <map>
#include <vector>

#include "Permutation.h"

using namespace std;
using namespace llvm;

// cl::opt<bool> analyse("analyse");
cl::opt<string> cmd("cmd");
cl::opt<string> fns("fns");
cl::opt<string> perms("perms");

struct InstructionPermutation : public ModulePass {
    static char ID;
    InstructionPermutation() : ModulePass(ID) {}

    vector<string>* split(string* s) {
        vector<string>* parts = new vector<string>();
        size_t current = s->find(",");
        size_t previous = 0;
        while(current != string::npos) {
            parts->push_back(s->substr(previous, current-previous));
            previous = current + 1;
            current = s->find(",", previous);
        }
        parts->push_back(s->substr(previous, current-previous));
        return parts;
    }

    bool runOnModule(Module &M) override {
        // Parse command line arguments
        vector<string>* functions = split(&fns);
        vector<string>* permutation_strings = split(&perms);

        // Convert permutation_strings to numbers
        map<string,int> permutationMap;
        for(size_t i=0; i<functions->size(); i++) {
            string fn = functions->at(i);
            string perm = permutation_strings->at(i);
            int perm_int = stoi(perm);
            errs() << perm << '\n';
            permutationMap.insert(pair<string,int>(fn, perm_int));
        }

        errs() << "Functions of interest:" << '\n';
        for(auto fn : *functions) {
            errs() << fn << '\n';
        }

        for(auto F = M.begin(); F != M.end(); F++) {
            int i = 0;
            for(auto BB = F->getBasicBlockList().begin(); BB != F->getBasicBlockList().end(); BB++) {
                string bbName(F->getName());
                bbName += "_" + to_string(i);

                if(find(functions->begin(), functions->end(), bbName) != functions->end()) {
                    errs() << "Important Basic Block found:" << '\n';

                    // Create permutation and add the instructions
                    Permutation perm = Permutation();
                    for(auto I = BB->getFirstInsertionPt(); I != BB->end(); I++) {
                        perm.addInstruction(&(*I));
                    }

                    // Add dependencies
                    Instruction* terminator = BB->getTerminator();
                    for(auto I = BB->getFirstInsertionPt(); I != BB->end(); I++) {
                        Instruction* inst = &(*I);

                        // The terminator must be the last instruction and therefore
                        // depends on all the others
                        if(inst != terminator) {
                            perm.addDependency(terminator, inst);
                        }

                        // If it is a call intruction, it depends on all previous
                        // instructions and all following instructions depend on it
                        if(strcmp(inst->getOpcodeName(), "call")) {
                            for (auto prevInst = BB->getFirstInsertionPt(); prevInst != I; prevInst++) {
                                perm.addDependency(inst, &(*prevInst));
                            }
                            for (auto nextInst = next(I); nextInst != BB->end(); nextInst++) {
                                perm.addDependency(&(*nextInst), inst);
                            }
                        }

                        // Add other dependencies
                        for (auto J = I->user_begin(); J != I->user_end(); J++) {
                            Instruction* user = cast<Instruction>(*J);

                            // Only dependencies within one basic block
                            if(user->getParent() != cast<BasicBlock>(BB)) {
                                continue;
                            }

                            // No dependencies to phi nodes
                            if(strcmp(user->getOpcodeName(), "phi") == 0) {
                                continue;
                            }

                            perm.addDependency(user, inst);
                        }
                    }

                    if(cmd == "count") {
                        int cnt = perm.countPermutations();
                        errs() << "Number of permutations: " << cnt << '\n';
                    } else if(cmd == "permute") {

                        int permutation = permutationMap[bbName];
                        errs() << "Using Permutation " << permutation << '\n';

                        list < Instruction * > insts = perm.getPermutation(permutation);
                        auto curr = insts.begin();
                        for (auto inst = next(insts.begin()); inst != insts.end(); inst++) {
                            (*inst)->moveAfter(*curr);
                            curr = inst;
                            errs() << (*inst)->getOpcodeName() << " ";
                        }
                        errs() << '\n';
                    }
                }
                i++;
            }
        }
        return true;
    }
};

char InstructionPermutation::ID = 0;
static RegisterPass<InstructionPermutation> X("instruction-permutation", "Instruction Permutation Pass", false, false);