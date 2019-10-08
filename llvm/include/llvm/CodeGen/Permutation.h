#ifndef LLVM_PERMUTATION_H
#define LLVM_PERMUTATION_H

#include <tuple>
#include <list>
#include <vector>
#include "llvm/CodeGen/ScheduleDAG.h"

using namespace std;
using namespace llvm;

template <class T>
class Permutation {
private:
    class DependencyGraph {
    public:
        enum Dep {
            NORMAL, DIRECT
        };

        list <tuple<T, T, Dep>> dependencies;

        void addDependency(T dependent, T independent);
        void addDependency(T dependent, T independent, Dep type);
        vector<T> getDirectDependencies(T independent);
    };

    class Schedule {
    public:
        list<T> instructions;

        Schedule() {}

        Schedule(Schedule &schedule);

        void scheduleInstruction(T i);

        list<T> toList();
    };

    class InstructionSet {
    public:
        list <T> instructions;

        void addInstruction(T i);

        vector <T> available(DependencyGraph *dg, Schedule *schedule);
    };

    DependencyGraph *dg;
    InstructionSet *is;

public:
    Permutation();

    void addInstruction(T inst);

    void addDependency(T a, T b);

    void addDirectDependency(T a, T b);

    int countPermutations();

    list <T> getPermutation(int permutation);

    list<T> getRandomPermutation();

    Schedule *permute(int *counter, int *stop, Schedule *schedule);
};

#endif //LLVM_PERMUTATION_H
