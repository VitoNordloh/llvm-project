#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/Permutation.h"
#include "llvm/CodeGen/ScheduleDAG.h"
#include "Permutations.h"

#include <iostream>
#include <list>
#include <stdlib.h>
#include <tuple>
#include <vector>

using namespace std;
using namespace llvm;

template <class T>
void Permutation<T>::DependencyGraph::addDependency(T dependent, T independent) {
    dependencies.push_back(make_tuple(dependent, independent, Permutation<T>::DependencyGraph::NORMAL));
}

template <class T>
void Permutation<T>::DependencyGraph::addDependency(T dependent, T independent, typename Permutation<T>::DependencyGraph::Dep type) {
    dependencies.push_back(make_tuple(dependent, independent, type));
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
list<T> Permutation<T>::Schedule::toList() {
    return instructions;
}

template <class T>
void Permutation<T>::InstructionSet::addInstruction(T i) {
    instructions.push_back(i);
}

template <class T>
vector <T> Permutation<T>::InstructionSet::available(Permutation::DependencyGraph *dg, Permutation::Schedule *schedule) {
    vector <T> avail;
    for (auto &inst : instructions) {
        if (find(schedule->instructions.begin(), schedule->instructions.end(), inst) !=
            schedule->instructions.end()) {
            continue;
        }

        bool valid = true;
        for (auto &dep : dg->dependencies) {
            T a = get<0>(dep);
            T b = get<1>(dep);
            if (a != inst) continue;

            if (find(schedule->instructions.begin(), schedule->instructions.end(), b) ==
                schedule->instructions.end()) {
                valid = false;
            }
        }
        if (valid) {
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
list <T> Permutation<T>::getPermutation(int permutation) {
    int counter = 0;
    int stop = permutation;
    Schedule *schedule = new Schedule();
    Schedule *result = permute(&counter, &stop, schedule);
    return result->toList();
}

template <class T>
list<T> Permutation<T>::getRandomPermutation() {
    Schedule *schedule = new Schedule();
    while(schedule->instructions.size() != is->instructions.size()) {
        vector <T> avail = is->available(dg, schedule);
        int i = rand() % avail.size();
        schedule->scheduleInstruction(avail[i]);

        // Schedule instructions which have a direct dependency
        for(auto &directInst : dg->getDirectDependencies(avail[i])) {
            schedule->scheduleInstruction(directInst);
        }
    }
    return schedule->toList();
}

template <class T>
typename Permutation<T>::Schedule *Permutation<T>::permute(int *counter, int *stop, Permutation::Schedule *schedule) {
    vector <T> avail = is->available(dg, schedule);
    for (auto &inst : avail) {
        Schedule *newSchedule = new Schedule(*schedule);
        newSchedule->scheduleInstruction(inst);

        // Schedule instructions which have a direct dependency
        for(auto &directInst : dg->getDirectDependencies(inst)) {
            newSchedule->scheduleInstruction(directInst);
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

template class Permutation<unsigned int>;
template class Permutation<MachineInstr*>;
