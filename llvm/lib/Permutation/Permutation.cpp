#include "llvm/Permutation/Permutation.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <list>
#include <string>
#include <tuple>
#include <vector>

using namespace std;
using namespace llvm;

template <class T>
Permutation<T>::DependencyGraph::DependencyGraph(const Permutation<T>::DependencyGraph &old) {
    dependencies = old.dependencies;
}

template <class T>
void Permutation<T>::DependencyGraph::clear() {
    dependencies.clear();
}

template <class T>
bool Permutation<T>::DependencyGraph::hasDependency(T dependent, T independent, typename Permutation<T>::DependencyGraph::Dep type) {
    for(auto &dep : dependencies) {
        T a = get<0>(dep);
        T b = get<1>(dep);
        auto c = get<2>(dep);
        if(a == dependent && b == independent && c == type) {
            return true;
        }
    }
    return false;
}

template <class T>
void Permutation<T>::DependencyGraph::addDependency(T dependent, T independent) {
    if(!hasDependency(dependent, independent, Permutation<T>::DependencyGraph::DependencyGraph::NORMAL)) {
        dependencies.push_back(make_tuple(dependent, independent, Permutation<T>::DependencyGraph::NORMAL));
    }
}

template <class T>
void Permutation<T>::DependencyGraph::addDependency(T dependent, T independent, typename Permutation<T>::DependencyGraph::Dep type) {
    if(!hasDependency(dependent, independent, type)) {
        dependencies.push_back(make_tuple(dependent, independent, type));
    }
}

template <class T>
vector<T> Permutation<T>::DependencyGraph::getDirectDependencies(T independent) {
    vector<T> directDependencies(0);
    for(auto &dep : dependencies) {
        T a = get<0>(dep);
        T b = get<1>(dep);
        if(b == independent && get<2>(dep) == DIRECT) {
            directDependencies.push_back(a);
        }
    }
    return directDependencies;
}

template <class T>
Permutation<T>::Schedule::Schedule(Permutation::Schedule &schedule) {
    instructions = list<T>(schedule.instructions);
}

template <class T>
void Permutation<T>::Schedule::scheduleInstruction(T i) {
    instructions.push_back(i);
}

template <class T>
bool Permutation<T>::Schedule::isScheduled(T i) {
    return find(instructions.begin(), instructions.end(), i) != instructions.end();
}

template <class T>
list<T> Permutation<T>::Schedule::toList() {
    return instructions;
}

template <class T>
Permutation<T>::InstructionSet::InstructionSet(const Permutation<T>::InstructionSet &old) {
    instructions = old.instructions;
}

template <class T>
void Permutation<T>::InstructionSet::clear() {
    instructions.clear();
}

template <class T>
void Permutation<T>::InstructionSet::addInstruction(T i) {
    instructions.push_back(i);
}

template <class T>
bool Permutation<T>::InstructionSet::depsFulfilled(T inst, Permutation::DependencyGraph *dg, Permutation::Schedule *schedule, list<T> additionalInsts) {
    bool valid = true;
    for (auto &dep : dg->dependencies) {
        T a = get<0>(dep);
        T b = get<1>(dep);
        if (a != inst) continue;

        bool inSchedule = false;
        if (find(schedule->instructions.begin(), schedule->instructions.end(), b) !=
            schedule->instructions.end()) {
            inSchedule = true;
        }

        bool inAdditional = false;
        if (find(additionalInsts.begin(), additionalInsts.end(), b) !=
            additionalInsts.end()) {
            inAdditional = true;
        }

        valid = valid && (inSchedule || inAdditional);
    }

    // There are also direct dependencies! An instruction can only be scheduled,
    // if the following direct dependencies can be scheduled
    bool directDepsFulfilled = true;
    vector<T> directDeps = dg->getDirectDependencies(inst);
    list<T> additionalInstsCopy = additionalInsts;
    additionalInstsCopy.push_back(inst);
    for(auto &directDep : directDeps) {
        directDepsFulfilled = directDepsFulfilled && depsFulfilled(directDep, dg, schedule, additionalInstsCopy);
    }

    return valid && directDepsFulfilled;
}

template <class T>
vector <T> Permutation<T>::InstructionSet::available(Permutation::DependencyGraph *dg, Permutation::Schedule *schedule) {
    vector <T> avail;
    for (auto &inst : instructions) {
        // Was the instruction already scheduled?
        if (find(schedule->instructions.begin(), schedule->instructions.end(), inst) !=
            schedule->instructions.end()) {
            continue;
        }

        // Are all deps fulfilled?
        if(depsFulfilled(inst, dg, schedule, list<T>(0))) {
            avail.push_back(inst);
        }
    }
    return avail;
}

template <class T>
Permutation<T>::Permutation() {
    dg = new Permutation::DependencyGraph();
    is = new Permutation::InstructionSet();
}

template <class T>
Permutation<T>::Permutation(const Permutation &old) {
    dg = new Permutation::DependencyGraph(*old.dg);
    is = new Permutation::InstructionSet(*old.is);
}

template <class T>
Permutation<T>::~Permutation() {
    delete dg;
    delete is;
}

template <class T>
void Permutation<T>::setLabelCallback(string (*newLabelFn) (T)) {
    labelFn = newLabelFn;
}

template <class T>
void Permutation<T>::clear() {
    dg->clear();
    is->clear();
}

template <class T>
void Permutation<T>::addInstruction(T inst) {
    is->addInstruction(inst);
}

template <class T>
void Permutation<T>::addDependency(T a, T b) {
    dg->addDependency(a, b);
}

template <class T>
void Permutation<T>::addDirectDependency(T a, T b) {
    dg->addDependency(a, b, Permutation<T>::DependencyGraph::DIRECT);
}

template <class T>
int Permutation<T>::countPermutations() {
    int counter = 0;
    int stop = -1;
    Schedule *schedule = new Schedule();
    permute(&counter, &stop, schedule);
    return counter;
}

template <class T>
typename Permutation<T>::Schedule* Permutation<T>::getPermutation() {
    dbgs() << "Getting first Permutation\n";
    Schedule *schedule = new Schedule();
    unsigned i = 0;
    while(schedule->instructions.size() != is->instructions.size()) {
        // Get available instructions
        auto avail = is->available(dg, schedule);
        dbgs() << "  " << avail.size() << " instructions available (";
        for(auto &inst : avail) {
            dbgs() << labelFn(inst) << " ";
        }
        dbgs() << ")\n";

        assert(!avail.empty() && "No available instructions");

        // Schedule first instruction
        dbgs() << "Scheduling " << labelFn(avail.front()) << "\n";
        scheduleInstruction(schedule, avail.front());

        // Dump graph
        dumpDot("graph_" + to_string(++i) + ".dot", schedule->toList());
    }
    return schedule;
}

template <class T>
typename Permutation<T>::Schedule* Permutation<T>::getPermutation(int permutation) {
    int counter = 0;
    int stop = permutation;
    Schedule *schedule = new Schedule();
    return permute(&counter, &stop, schedule);
}

template <class T>
list<T> Permutation<T>::getRandomPermutation() {
    Schedule *schedule = new Schedule();
    while(schedule->instructions.size() != is->instructions.size()) {
        vector <T> avail = is->available(dg, schedule);
        int i = rand() % avail.size();
        scheduleInstruction(schedule, avail[i]);
    }
    return schedule->toList();
}

template <class T>
typename Permutation<T>::Schedule *Permutation<T>::permute(int *counter, int *stop, Permutation::Schedule *schedule) {
    vector <T> avail = is->available(dg, schedule);
    for (auto &inst : avail) {
        auto *newSchedule = new Schedule(*schedule);
        if(!scheduleInstruction(newSchedule, inst)) {
            delete newSchedule;
            continue;
        }

        if (newSchedule->instructions.size() == is->instructions.size()) {
            (*counter)++;
            if (*stop == 0) {
                return newSchedule;
            } else {
                (*stop)--;
            }
        } else {
            Schedule *result = permute(counter, stop, newSchedule);
            if (result != nullptr) {
                return result;
            }
        }

        delete newSchedule;
    }
    return nullptr;
}

template <class T>
bool Permutation<T>::scheduleInstruction(Schedule *schedule, T inst) {
    if(labelFn != nullptr) {
        dbgs() << "Scheduling instruction " << labelFn(inst) << "\n";
    }
    schedule->scheduleInstruction(inst);

    // Get available instructions
    vector<T> avail = is->available(dg, schedule);

    // Schedule instructions which have a direct dependency
    for(auto &directInst : dg->getDirectDependencies(inst)) {
        if(labelFn != nullptr) {
            dbgs() << "  Direct dependency: " << labelFn(directInst) << "\n";
        }
        // Is the instruction available?
        if(find(avail.begin(), avail.end(), directInst) == avail.end()) {
            if(labelFn != nullptr) {
                dbgs() << "    Not available";
            }
            return false;
        }
        scheduleInstruction(schedule, directInst);
    }
    return true;
}

template <class T>
void Permutation<T>::dumpDot(string filename, list<T> nodesToExclude) {
    ofstream file(filename, ios_base::out | ios_base::trunc);
    file << "digraph G {" << endl;

    // Dump nodes
    for(auto &I : is->instructions) {
        if(find(nodesToExclude.begin(), nodesToExclude.end(), I) == nodesToExclude.end()) {
            file << labelFn(I) << " [shape=box];" << endl;
        }
    }

    // Dump dependencies
    for(auto &dep : dg->dependencies) {
        T dependent = get<0>(dep);
        T independent = get<1>(dep);

        if(find(nodesToExclude.begin(), nodesToExclude.end(), independent) != nodesToExclude.end() ||
            find(nodesToExclude.begin(), nodesToExclude.end(), dependent) != nodesToExclude.end()) {
            continue;
        }

        unsigned type = get<2>(dep);
        if(type == DependencyGraph::NORMAL) {
            file << "edge [color=black];" << endl;
        } else {
            file << "edge [color=red];" << endl;
        }
        file << labelFn(independent) << " -> " << labelFn(dependent) << ";" << endl;
    }

    file << "}";
    file.close();
}

template class Permutation<unsigned int>;
