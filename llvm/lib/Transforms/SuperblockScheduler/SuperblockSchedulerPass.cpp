#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/DominanceFrontier.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Pass.h"
#include "llvm/PassAnalysisSupport.h"
#include "Permutation.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/ProfileReader/ProfileReader.h"
#include "llvm/Transforms/SuperblockFinder/SuperblockFinder.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"

#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace llvm;
using namespace std;

#define DEBUG_TYPE "superblock-scheduler"

static cl::opt<bool> WritePermutations("write-permutations", cl::desc("Write Permutations."));
static cl::opt<string> PermutationMap("permutation-map", cl::desc("Permutation."));

namespace {
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

    string getNodeLabel(unsigned id) {
        return to_string(id);
    }

    struct SNode {
    public:
        enum Type {
            BBStart, BBEnd, Inst
        };

        unsigned id;
        unsigned BBId;
        unsigned type;
        bool mustBeFirst = false;
        bool mustBeLast = false;
        BasicBlock *BB;
        Instruction *instr;
        SNode *endBB;
    };

    struct SuperblockScheduler : FunctionPass {
        static char ID;

        SuperblockScheduler() : FunctionPass(ID) {
            DI = nullptr;
        }
        bool runOnFunction(Function &F) override;
        void getAnalysisUsage(AnalysisUsage &AU) const override;

    private:
        void initSNodes();
        unsigned hasDep(Instruction *A, Instruction *B);
        void buildDepGraph();
        bool inc(vector<unsigned> *map, vector<unsigned>::iterator it);
        signed InstMovement(BasicBlock *before, BasicBlock *after);
        void compensateUpwardsMovement(SNode *node, BasicBlock *before, BasicBlock *after);
        void compensateDownwardsMovement(SNode *node, BasicBlock *before, BasicBlock *after);
        void schedule();
        void dumpSchedule();
        static void dumpSNode(SNode *snode);

        Function *F;
        DependenceInfo *DI;
        vector<SNode*> snodes;
        map<unsigned, SNode*> snodeMap;
        vector<BasicBlock*> SB;
        Permutation<unsigned> perm;
        list<unsigned> newSchedule;
    };

    bool SuperblockScheduler::runOnFunction(Function &F) {
        if(!F.getName().equals("pat_search")) {
            dbgs() << "Skipping " << F.getName() << "\n";
            return false;
        }

        this->F = &F;
        DI = &getAnalysis<DependenceAnalysisWrapperPass>().getDI();
        SB = getAnalysis<SuperblockFinder>().getSB();

        initSNodes();
        buildDepGraph();
        schedule();
        dumpSchedule();

        return true;
    }

    void SuperblockScheduler::getAnalysisUsage(AnalysisUsage &AU) const {
        AU.setPreservesCFG();
        AU.addRequired<DependenceAnalysisWrapperPass>();
        AU.addRequired<SuperblockFinder>();
    }

    void SuperblockScheduler::initSNodes() {
        snodes.clear();
        snodeMap.clear();
        unsigned id = 0;
        unsigned BBId = 0;
        for(auto &BB : SB) {
            // Create SNode for the start of the BasicBlock
            auto *BBStart = new SNode();
            BBStart->id = ++id;
            BBStart->BBId = ++BBId;
            BBStart->mustBeFirst = BB == SB.front();
            BBStart->BB = BB;
            BBStart->type = SNode::BBStart;
            snodes.push_back(BBStart);
            snodeMap.insert(pair<unsigned, SNode*>(BBStart->id, BBStart));

            for(Instruction &I : *BB) {
                // Create SNode for the instruction
                auto *node = new SNode();
                node->id = ++id;
                node->instr = &I;
                node->BB = BB;
                node->BBId = BBStart->id;
                node->type = SNode::Inst;
                snodes.push_back(node);
                snodeMap.insert(pair<unsigned, SNode*>(node->id, node));
            }

            // Create SNode for the end of the BasicBlock
            auto *BBEnd = new SNode();
            BBEnd->id = ++id;
            BBEnd->BBId = BBStart->BBId;
            BBEnd->mustBeLast = BB == SB.back();
            BBEnd->BB = BB;
            BBEnd->type = SNode::BBEnd;
            snodes.push_back(BBEnd);
            snodeMap.insert(pair<unsigned, SNode*>(BBEnd->id, BBEnd));

            // Update references
            BBStart->endBB = BBEnd;
        }
    }

    unsigned SuperblockScheduler::hasDep(Instruction *A, Instruction *B) {
        // If B uses A, there is a dependency
        if(find(A->user_begin(), A->user_end(), B) != A->user_end()) {
            return 1;
        }

        // If A or B reads/writes from/to memory, there could be a dependency
        if(DI->depends(A, B, true)) {
            return 2;
        }

        // If A or B has side effects, there could be a dependency
        if(A->mayThrow() || B->mayThrow()) {
            return 3;
        }

        // If A or B is a call, there could be a dependency
        if(A->isFenceLike() || B->isFenceLike()) {
            return 4;
        }

        return 0;
    }

    void SuperblockScheduler::buildDepGraph() {
        perm.clear();

        // Add all instructions
        for(auto &node : snodes) {
            perm.addInstruction(node->id);
        }

        // Find dependencies
        for(auto itA = snodes.begin(); itA != snodes.end(); ++itA) {
            SNode *A = *itA;
            for(auto itB = next(itA); itB != snodes.end(); ++itB) {
                SNode *B = *itB;

                /**
                 * Make sure that the SNode with mustBeFirst=true will be
                 * the first node.
                 */
                if(A->mustBeFirst) {
                    perm.addDependency(B->id, A->id);
                }

                /**
                 * Make sure that the SNode with mustBeLast=true will be
                 * the last node.
                 */
                if(B->mustBeLast) {
                    perm.addDependency(B->id, A->id);
                }

                /**
                 * Make sure that there is a direct dependency between
                 * the end and start of two consecutive MBBs.
                 */
                if(A->type == SNode::BBEnd && next(itA) == itB) {
                    perm.addDirectDependency(B->id, A->id);
                }

                if(A->type == SNode::Inst && B->type == SNode::Inst) {
                    /**
                     * Dependency between two instructions:
                     * S1: a =
                     * S2:   = a
                     */
                    Instruction *IA = A->instr;
                    Instruction *IB = B->instr;
                    if(hasDep(IA, IB)) {
                        perm.addDependency(B->id, A->id);
                    }
                }

                if((A->type == SNode::BBStart || A->type == SNode::BBEnd) &&
                    (B->type == SNode::BBStart || B->type == SNode::BBEnd)) {
                    /**
                     * The order of the helping nodes BBStart/BBEnd must not be changed:
                     * -- BB1 Start --
                     *    S1
                     *    S2
                     * -- BB1 End   --
                     * -- BB2 Start --
                     *    S3
                     * -- BB2 End   --
                     */
                    perm.addDependency(B->id, A->id);
                }

                if((A->type == SNode::BBStart && B->type == SNode::Inst)) {
                    /**
                     * PHI Nodes must be scheduled directly at the start of the BasicBlock.
                     */
                    if(B->instr->getOpcode() == Instruction::PHI && B->BB == A->BB) {
                        perm.addDirectDependency(B->id, A->id);
                    }
                }

                if(A->type == SNode::BBStart && B->type == SNode::Inst) {
                    /**
                     * It might be dangerous to move stores and loads upwards into other branches
                     * where the store does not get executed usually (the dependencies between the
                     * store and all other instruction within the new path would have to be determined).
                     */
                     if(B->instr->mayReadOrWriteMemory()) {
                         perm.addDependency(B->id, A->id);
                     }
                }

                if(A->type == SNode::Inst && B->type == SNode::BBEnd) {
                    /**
                     * Terminators of a BasicBlock always have to be the last instruction.
                     */
                    if(A->instr->isTerminator() && A->instr->getParent() == B->BB) {
                        perm.addDirectDependency(B->id, A->id);
                    }
                }
            }
        }
    }

    bool SuperblockScheduler::inc(vector<unsigned> *map, vector<unsigned>::iterator it) {
        unsigned numBlocks = SB.size();
        if(it == map->end()) return false;
        if(*it == numBlocks-1) {
            *it = 0;
            return inc(map, next(it));
        } else {
            ++(*it);
        }
        return true;
    }

    signed SuperblockScheduler::InstMovement(BasicBlock *before, BasicBlock *after) {
        // No change
        if(before == after) {
            return 0;
        }

        for(auto &BB : SB) {
            if(BB == after) {
                return -1; // 'after' comes before 'before' => inst was moved upwards
            }
            if(BB == before) {
                return 1; // 'before' comes after 'after' => inst was moved downwards
            }
        }

        return 0;
    }

    void SuperblockScheduler::compensateUpwardsMovement(SNode *node, BasicBlock *before, BasicBlock *after) {
        // TODO: Do we have to compensate anything?
    }

    void SuperblockScheduler::compensateDownwardsMovement(SNode *node, BasicBlock *before, BasicBlock *after) {
        dbgs() << "Instruction was moved downwards:\n";
        dbgs() << "  ";
        node->instr->print(dbgs());
        dbgs() << "\n";

        auto itStart = find(SB.begin(), SB.end(), before);
        auto itEnd = find(SB.begin(), SB.end(), after);
        vector<BasicBlock*> BBs(0);
        dbgs() << "  Successors: ";
        for(auto it = itStart; it != itEnd; ++it) {
            dbgs() << (*it)->getName() << " ";
            BBs.push_back(*it);
        }
        dbgs() << "\n";

        // Insert a copy of the instruction into all successors
        vector<Instruction*> insertedCopies(0);
        for(auto &BB : BBs) {
            for(auto succ : successors(BB)) {
                // If the successor is part of the Superblock, no compensation code has to
                // be inserted.
                if(find(SB.begin(), SB.end(), succ) != SB.end()) {
                    continue;
                }

                // Clone instruction
                Instruction *clone2 = node->instr->clone();
                clone2->insertBefore(succ->getFirstNonPHIOrDbg());
                insertedCopies.push_back(clone2);
            }
        }

        // If the instruction does not return a value, we do not have to compensate anything.
        // Otherwise, we have to update the SSA form.
        if(node->instr->getType()->isVoidTy()) {
            return;
        }

        // Insert a copy of the instruction right above the original one. This makes it easier
        // to update all uses to the correct def.
        Instruction *clone = node->instr->clone();
        clone->insertBefore(node->instr);
        insertedCopies.push_back(clone);

        // Update the SSA form
        auto ssa = new SSAUpdater();
        ssa->Initialize(node->instr->getType(), node->instr->getName());
        for(auto def : insertedCopies) {
            ssa->AddAvailableValue(def->getParent(), def);
        }
        while(!node->instr->use_empty()) {
            ssa->RewriteUseAfterInsertions(*node->instr->use_begin());
        }

        // Remove the original instruction
        node->instr->eraseFromParent();

        // Replace the instruction in the SNode
        node->instr = clone;
    }

    void SuperblockScheduler::schedule() {
        // Print snodes
        for(auto *node : snodes) {
            dumpSNode(node);
        }

        // Count Instructions and create maps
        unsigned instI = 0, BBI = 0;
        map<unsigned, SNode*> instMap, BBMap;

        for(auto *node : snodes) {
            if(node->type == SNode::Inst) {
                if(node->instr->isFenceLike() || node->instr->isTerminator() || node->instr->getOpcode() == Instruction::PHI) {
                    continue;
                }
                instMap.insert(pair<unsigned, SNode*>(instI++, node));
            } else if(node->type == SNode::BBStart) {
                BBMap.insert(pair<unsigned, SNode*>(BBI++, node));
            }
        }

        /**
         * This section searches for valid permutations.
         */
        if(WritePermutations) {
            dbgs() << pow(BBI, instI) << " possible permutations\n";

            list<map<unsigned, unsigned>*> foundPerms;

            srand(time(nullptr));
            ofstream file("permutations.txt", ios_base::out | ios_base::trunc);
            unsigned k = 0, valid = 0;
            while(k < 100000000) {
                // Print counter
                if (k % 100 == 0) {
                    dbgs() << k << "\n";
                }

                // Get permutation
                auto sched = perm.getRandomPermutation();
                /*if(schedTmp == nullptr) {
                    break;
                }*/
                // auto sched = schedTmp->toList();

                // Inc k
                k++;

                // Create mapping of inst to BB
                unsigned curBB = 9999999;
                auto *mapping = new map<unsigned, unsigned>();
                bool isUnique = false;
                for(auto &i : sched) {
                    SNode *node = snodeMap[i];
                    if(node->type == SNode::BBStart) {
                        curBB = node->id;
                    } else if(node->type == SNode::Inst) {
                        mapping->insert(pair<unsigned, unsigned>(node->id, curBB));
                        if(isUnique) continue;
                        if(foundPerms.empty()) isUnique = true;
                        bool allUnique = true;
                        for(auto &found : foundPerms) {
                            unsigned BBId = found->at(node->id);
                            allUnique = allUnique && (BBId != curBB);
                        }
                        isUnique = isUnique || allUnique;
                    }
                }

                if(isUnique) {
                    foundPerms.push_back(mapping);
                    for(auto &m : *mapping) {
                        file << m.first << "->" << m.second << ",";
                        dbgs() << m.first << "->" << m.second << " ";
                    }
                    file << endl;
                    dbgs() << "\n";
                    valid++;
                    dbgs() << "Found (" << valid << ")\n";
                }

            }
            file.close();
            return;
        }

        // Add additional dependencies
        dbgs() << "PermutationMap: " << PermutationMap << "\n";
        vector<string> *mapping = split(&PermutationMap, ",");
        for(auto &mapEntry : *mapping) {
            vector<string> *ids = split(&mapEntry, ":");

            unsigned instId = stoi(ids->at(0));
            unsigned startBBId = stoi(ids->at(1));

            dbgs() << "Getting " << instId << ":" << startBBId << "\n";

            SNode *inst = snodeMap[instId];
            SNode *startBB = snodeMap[startBBId];
            SNode *endBB = startBB->endBB;

            assert(inst != nullptr && "Inst is null");
            assert(startBB != nullptr && "startBB is null");
            dbgs() << "Checking endBB of " << startBB->BB->getName() << "\n";
            if(startBB->type == SNode::Inst) {
                dbgs() << "Is Inst\n";
            } else if(startBB->type == SNode::BBStart) {
                dbgs() << "Is Start\n";
            } else if(startBB->type == SNode::BBEnd) {
                dbgs() << "Is end\n";
            }
            assert(endBB != nullptr && "endBB is null");

            perm.addDependency(inst->id, startBB->id);
            perm.addDependency(endBB->id, inst->id);
        }
        dbgs() << "Added all dependencies\n";

        perm.setLabelCallback(getNodeLabel);

        perm.dumpDot("graph.dot", list<unsigned>(0));

        dbgs() << "Exported\n";

        newSchedule = perm.getPermutation()->toList();

        dbgs() << "Created schedule\n";

        // Traverse the new schedule bottom up and insert the instructions into
        // the BasicBlocks.
        BasicBlock *BB = nullptr;
        for(auto id = newSchedule.rbegin(); id != newSchedule.rend(); ++id) {
            SNode *node = snodeMap[*id];
            if(node->type == SNode::BBEnd) {
                BB = node->BB;
            } else if(node->type == SNode::Inst) {
                // Move instruction
                BB->getInstList().splice(BB->getInstList().begin(), node->instr->getParent()->getInstList(), node->instr->getIterator());

                // Insert compensation code
                switch(InstMovement(node->BB, BB)) {
                    case -1: // Inst moved upwards
                        compensateUpwardsMovement(node, node->BB, BB);
                        break;
                    case 0:
                    default:
                        break;
                    case 1: // Inst moved downwards
                        compensateDownwardsMovement(node, node->BB, BB);
                        break;
                }
            }
        }

        dbgs() << "Created new schedule\n";
    }

    void SuperblockScheduler::dumpSchedule() {
        for(auto &id : newSchedule) {
            dumpSNode(snodeMap[id]);
        }
    }

    void SuperblockScheduler::dumpSNode(SNode *snode) {
        string sId = to_string(snode->id);
        string s;
        s.insert(s.begin(), 4 - sId.length(), ' ');
        s.append(sId);
        dbgs() << "(" << s << "): ";

        string last, first;
        if(snode->mustBeFirst) {
            first = "(Must be first)";
        }
        if(snode->mustBeLast) {
            last = "(Must be last)";
        }

        if(snode->type == SNode::BBStart) {
            dbgs() << "==== " << snode->BBId << " ==== " << first << " (Preds: " << pred_size(snode->BB) << ", Succs: " << succ_size(snode->BB) << ")\n";
        } else if(snode->type == SNode::BBEnd) {
            dbgs() << "#### " << snode->BBId << " #### " << last << "\n\n";
        } else {
            dbgs() << "   ";
            snode->instr->print(dbgs());
            dbgs() << "\n";
        }
    }
}

char SuperblockScheduler::ID = 0;
static RegisterPass<SuperblockScheduler> X("superblock-scheduler", "Superblock Scheduler.");