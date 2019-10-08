#include "llvm/ADT/DenseMap.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineDominanceFrontier.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/Permutation.h"
#include "llvm/CodeGen/ScheduleDAG.h"
#include "llvm/CodeGen/ScheduleDAGInstrs.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "Permutations.h"

#include <iostream>
#include <map>
#include <stdlib.h>
#include <time.h>
#include <vector>

using namespace std;
using namespace llvm;

#define DEBUG_TYPE "global-scheduler"

static cl::opt<bool> EnableGlobalPermutation(
        "enable-global-permutation",
        cl::desc("Enable global permutation scheduler."),
        cl::init(false), cl::Hidden);

static cl::opt<int> PermutationInit("global-permutation-init", cl::Hidden);

namespace {
    class SNode {
    public:
        enum Kind {
            Instruction, BBStart, BBEnd
        };

        unsigned id;
        unsigned kind;
        bool mustBeFirst = 0;
        bool mustBeLast = 0;
        MachineInstr* instr;
        MachineBasicBlock* MBB;
    };

    /**
     * Class which implements the Superblock Scheduler.
     */
    class GlobalScheduler : public MachineFunctionPass {
    public:
        GlobalScheduler() : MachineFunctionPass(ID) {
            initializeGlobalSchedulerPass(*PassRegistry::getPassRegistry());
        }

        bool runOnMachineFunction(MachineFunction&) override;
        void getAnalysisUsage(AnalysisUsage &AU) const override;
        static char ID; // Class identification, replacement for typeinfo

    private:
        MachineFunction* MF;
        const TargetRegisterInfo* TRI;
        vector<MachineBasicBlock*> SB;
        vector<SNode*> snodes;
        map<unsigned, SNode*> snode_map;

        enum Dep {
            RAW = 1, WAR = 2, WAW = 4, Other = 8
        };

        vector<unsigned> getRegAliases(const MachineOperand *MO);
        unsigned getDependency(MachineInstr* A, MachineInstr* B);
        Permutation<unsigned>* buildSchedGraph();
        vector<MachineBasicBlock*> findSuperblock(MachineBasicBlock *MBB);
        void initInstrs();
        void dumpPermutation(Permutation<unsigned> *perm, unsigned id);
        void dumpSB();
        void applyPermutation(Permutation<unsigned> *perm, unsigned id);
        signed MBBPosition(MachineBasicBlock *A, MachineBasicBlock *B);
    };

    bool GlobalScheduler::runOnMachineFunction(MachineFunction &MF) {
        dbgs() << "Running on Machine Function: " << MF.getName() << "\n";
        if (!MF.getName().equals("loop")) {
            dbgs() << "Skipping\n";
            return false;
        }

        this->MF = &MF;
        TRI = this->MF->getSubtarget().getRegisterInfo();
        initInstrs();
        Permutation<unsigned> *perm = buildSchedGraph();

        if(EnableGlobalPermutation) {
            dbgs() << "Do schedule globally\n";

            unsigned init = time(nullptr);
            if(PermutationInit >= 0) {
                init = PermutationInit;
            }
            dbgs() << "Using " << init << " as init\n";

            applyPermutation(perm, init);
            dumpPermutation(perm, init);
            dumpSB();
        } else {
            dbgs() << "Do not schedule globally\n";
        }
        return EnableGlobalPermutation;
    }

    void GlobalScheduler::getAnalysisUsage(AnalysisUsage &AU) const {
        AU.addRequired<LiveIntervals>();
        // AU.addPreserved<LiveIntervals>();
        MachineFunctionPass::getAnalysisUsage(AU);
    }

    vector<unsigned> GlobalScheduler::getRegAliases(const MachineOperand *MO) {
        unsigned reg = MO->getReg();
        vector<unsigned> list = {};
        if(TargetRegisterInfo::isPhysicalRegister(reg)) {
            for(MCRegAliasIterator Alias(reg, TRI, true); Alias.isValid(); ++Alias) {
                list.push_back(*Alias);
            }
        } else {
            list.push_back(reg);
        }
        return list;
    }

    unsigned GlobalScheduler::getDependency(MachineInstr *A, MachineInstr *B) {
        unsigned dependencies = 0;
        for(unsigned j = 0; j < A->getNumOperands(); ++j) {
            const MachineOperand &MOA = A->getOperand(j);

            // Operations on registers
            if(MOA.isReg()) {
                // Iterate over all registers of A
                vector<unsigned> regsA = getRegAliases(&MOA);

                for(auto &regA : regsA) {
                    // Iterate over all operands of B
                    for(unsigned i = 0; i < B->getNumOperands(); ++i) {
                        const MachineOperand &MOB = B->getOperand(i);

                        // We only care for registers
                        if(MOB.isReg()) {
                            // Iterate over all registers of B
                            vector<unsigned> regsB = getRegAliases(&MOB);
                            for(auto &regB : regsB) {
                                if(regA == regB) {
                                    if(MOA.isDef() && MOB.isUse()) {
                                        dependencies |= RAW;
                                    } else if(MOA.isUse() && MOB.isDef()) {
                                        dependencies |= WAR;
                                    } else if(MOA.isDef() && MOB.isDef()) {
                                        dependencies |= WAW;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Memory manipulations
            if(A->mayStore() && B->mayLoad()) {
                dependencies |= RAW;
            } else if(A->mayStore() && B->mayStore()) {
                dependencies |= WAW;
            } else if(A->mayLoad() && B->mayStore()) {
                dependencies |= WAR;
            }

            // Calls
            if(A->isCall() || B->isCall()) {
                dependencies |= Other;
            }
        }
        return dependencies;
    }

    Permutation<unsigned>* GlobalScheduler::buildSchedGraph() {
        Permutation<unsigned> *perm = new Permutation<unsigned>();

        // Add all available instructions (respectively their IDs)
        for(auto &node : snodes) {
            perm->addInstruction(node->id);
        }

        // Add dependencies
        for(vector<SNode*>::iterator NodeA = snodes.begin(); NodeA != snodes.end(); ++NodeA) {
            for(vector<SNode*>::iterator NodeB = next(NodeA); NodeB != snodes.end(); ++NodeB) {

                /**
                 * Make sure that the SNode with mustBeFirst=true will be
                 * the first node.
                 */
                if((*NodeA)->mustBeFirst) {
                    perm->addDependency((*NodeB)->id, (*NodeA)->id);
                }

                /**
                 * Make sure that the SNode with mustBeLast=true will be
                 * the last node.
                 */
                if((*NodeB)->mustBeLast) {
                    perm->addDependency((*NodeB)->id, (*NodeA)->id);
                }

                /**
                 * Make sure that there is a direct dependency between
                 * the end and start of two consecutive MBBs.
                 */
                if((*NodeA)->kind == SNode::BBEnd && next(NodeA) == NodeB) {
                    perm->addDirectDependency((*NodeB)->id, (*NodeA)->id);
                }

                if((*NodeA)->kind == SNode::Instruction && (*NodeB)->kind == SNode::Instruction) {
                    /**
                     * Dependency between two instructions:
                     * S1: a =
                     * S2:   = a
                     */
                    MachineInstr *MIA = (*NodeA)->instr;
                    MachineInstr *MIB = (*NodeB)->instr;
                    if(getDependency(MIA, MIB) != 0) {
                        perm->addDependency((*NodeB)->id, (*NodeA)->id);
                    }
                }

                if((*NodeA)->kind == SNode::BBStart && (*NodeB)->kind == SNode::BBStart) {
                    /**
                     * The order of the helping nodes BBStart/BBEnd must not be changed:
                     * -- BB1 Start --
                     * S1
                     * S2
                     * -- BB1 End   --
                     * -- BB2 Start --
                     * S3
                     * -- BB2 End   --
                     */
                    perm->addDependency((*NodeB)->id, (*NodeA)->id);
                }

                if((*NodeA)->kind == SNode::Instruction && (*NodeB)->kind == SNode::BBEnd) {
                    /**
                     * There might be a dependency between an instruction and a terminator.
                     * Since the terminators are not inserted directly, but within an
                     * SNode::BBEnd block, they have to be checked explicitly.
                     */
                     for(auto &T : (*NodeB)->MBB->terminators()) {
                         if(getDependency((*NodeA)->instr, &T) != 0) {
                             perm->addDependency((*NodeB)->id, (*NodeA)->id);
                         }
                     }
                }
            }
        }
        return perm;
    }

    /**
     * Finds a superblock, starting at MBB.
     */
    vector<MachineBasicBlock*> GlobalScheduler::findSuperblock(MachineBasicBlock *MBB) {
        vector<MachineBasicBlock*> MBBS(0);
        MBBS.push_back(MBB);

        MachineBasicBlock* curr = MBB;
        while(true) {
            // The block is only allowed to have one predecessor
            if(curr->pred_size() > 1) {
                break;
            }

            // Add the current MBB to the superblock
            MBBS.push_back(curr);

            // We need a successor
            if(curr->succ_size() < 1) {
                break;
            }

            // Update curr
            curr = *(curr->successors().begin());
        };
        return MBBS;
    }

    void GlobalScheduler::dumpSB() {
        dbgs() << "Superblock:\n";
        for(auto &MBB : SB) {
            MBB->print(dbgs());
        }
        dbgs() << "\n";
    }

    void GlobalScheduler::initInstrs() {
        // Find largest superblock;
        size_t maxLength = 0;
        for(auto &MBB : *MF) {
            auto newSB = findSuperblock(&MBB);
            if(newSB.size() > maxLength) {
                SB = newSB;
                maxLength = SB.size();
            }
        }

        dumpSB();

        // Create SNode for every instruction
        snodes.clear();
        unsigned id = 0;
        for(auto &MBB : SB) {
            dbgs() << "Found MBB: " << MBB->getName() << "\n";

            // Create start for MBB
            SNode *bbstart = new SNode();
            bbstart->kind = SNode::BBStart;
            bbstart->id = ++id;
            bbstart->mustBeFirst = bbstart->id == 1;
            bbstart->MBB = MBB;
            snodes.push_back(bbstart);
            snode_map.insert(pair<unsigned, SNode*>(bbstart->id, bbstart));

            for(auto &MI : *MBB) {
                // Terminators have to be skipped
                if(MI.isTerminator()) {
                    continue;
                }

                // Create SNode for instruction
                SNode *node = new SNode();
                node->kind = SNode::Instruction;
                node->id = ++id;
                node->instr = &MI;
                snodes.push_back(node);
                snode_map.insert(pair<unsigned, SNode*>(node->id, node));
            }

            // Create end for MBB
            SNode *bbend = new SNode();
            bbend->kind = SNode::BBEnd;
            bbend->id = ++id;
            bbend->MBB = MBB;
            snodes.push_back(bbend);
            snode_map.insert(pair<unsigned, SNode*>(bbend->id, bbend));
        }

        // Set mustBeLast for last node
        snodes.back()->mustBeLast = true;
    }

    void GlobalScheduler::dumpPermutation(Permutation<unsigned> *perm, unsigned id) {
        srand(id);
        list<unsigned> permutation = perm->getRandomPermutation();
        for(auto &id : permutation) {
            SNode* snode = snode_map[id];
            if(snode->kind == SNode::BBStart) {
                dbgs() << "==== " << snode->MBB->getName() << " ====\n";
            } else if(snode->kind == SNode::BBEnd) {
                for(auto &T : snode->MBB->terminators()) {
                    dbgs() << "   ";
                    T.print(dbgs());
                }
                dbgs() << "#### " << snode->MBB->getName() << " ####\n\n";
            } else {
                dbgs() << "   ";
                snode->instr->print(dbgs());
            }
        }
        dbgs() << "Dump end\n";
    }

    /**
     * Determines whether B is a successor of A.
     */
    signed GlobalScheduler::MBBPosition(MachineBasicBlock *A, MachineBasicBlock *B) {
        if(A == B) return 0;
        for(auto &MBB : SB) {
            if(MBB == A) return -1;
            if(MBB == B) return 1;
        }
        assert(false && "MBBs was not found in Superblock");
        return 0;
    }

    void GlobalScheduler::applyPermutation(Permutation<unsigned> *perm, unsigned id) {
        srand(id);
        list<unsigned> permutation = perm->getRandomPermutation();

        MachineBasicBlock *MBB = nullptr;
        MachineInstr *lastInsert = nullptr;
        for(auto &id : permutation) {
            SNode* snode = snode_map[id];
            if(snode->kind == SNode::BBStart) {
                MBB = snode->MBB;
                lastInsert = nullptr;
            } else if(snode->kind == SNode::Instruction) {
                // Find the iterator, where the instruction has to be inserted
                MachineBasicBlock::iterator insertPoint = MBB->begin();
                if(lastInsert != nullptr) {
                    insertPoint = lastInsert->getIterator();
                }
                lastInsert = snode->instr;

                // Insert instruction
                MBB->splice(next(insertPoint), snode->instr->getParent(), snode->instr->getIterator());

                switch(MBBPosition(snode->MBB, MBB)) {
                    case(-1): // Instruction was moved downwards
                        for(auto &testMBB : SB) {
                            // testMBB must be equal or below the original MBB
                            bool a = MBBPosition(snode->MBB, testMBB) >= 0;
                            // testMBB must be less than the new MBB
                            bool b = MBBPosition(testMBB, MBB) == -1;
                            if(a && b) {
                                for(auto &succ : testMBB->successors()) {
                                    // Duplicate instruction and insert it in all
                                    // successors which are not part of the SB.
                                    if(find(SB.begin(), SB.end(), succ) == SB.end()) {
                                        MachineInstr *copy = MF->CloneMachineInstr(&(*(snode->instr)));
                                        succ->insert(succ->begin(), copy);
                                    }
                                }
                            }
                        }
                    default:
                    case(0): // Same MBB
                        continue;
                    case(1): // Instruction was moved upwards
                        for(auto &testMBB : SB) {
                            // testMBB must be below the new MBB
                            bool a = MBBPosition(MBB, testMBB) == -1;
                            if(a) {
                                // Insert Live-Ins
                                for(auto &succ : testMBB->successors()) {
                                    for(auto &MO : snode->instr->operands()) {
                                        unsigned reg = MO.getReg();
                                        if(TargetRegisterInfo::isPhysicalRegister(reg)) {
                                            succ->addLiveIn(reg);
                                        }
                                    }
                                }
                            }
                        }
                        break;
                }
            }
        }
    }
}

char GlobalScheduler::ID = 0;
char &llvm::GlobalSchedulerID = GlobalScheduler::ID;

INITIALIZE_PASS_BEGIN(GlobalScheduler, DEBUG_TYPE, "GlobalScheduler", false, false)
INITIALIZE_PASS_DEPENDENCY(LiveIntervals)
INITIALIZE_PASS_END(GlobalScheduler, DEBUG_TYPE, "GlobalScheduler", false, false)