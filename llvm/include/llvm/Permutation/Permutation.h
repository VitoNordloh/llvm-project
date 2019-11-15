#ifndef LLVM_PERMUTATION_H
#define LLVM_PERMUTATION_H

#include <string>
#include <tuple>
#include <list>
#include <vector>

using namespace std;

template <class T>
class Permutation {
private:
    class DependencyGraph {
    public:
        enum Dep {
            NORMAL, DIRECT
        };

        list <tuple<T, T, Dep>> dependencies;

        DependencyGraph() = default;
        DependencyGraph(const DependencyGraph &old);
        void clear();
        bool hasDependency(T dependent, T independent, Dep type);
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
        bool isScheduled(T i);

        list<T> toList();
    };

    class InstructionSet {
    public:
        InstructionSet() = default;
        InstructionSet(const InstructionSet &old);
        
        list <T> instructions;

        void clear();

        void addInstruction(T i);

        bool depsFulfilled(T inst, Permutation::DependencyGraph *dg, Permutation::Schedule *schedule, list<T> additionalInsts);

        vector <T> available(DependencyGraph *dg, Schedule *schedule);

        bool directDependenciesFulfilled(T inst, DependencyGraph *dg, Schedule *schedule, list<T> previous);
    };

    string (*labelFn) (T) = nullptr;
    DependencyGraph *dg;
    InstructionSet *is;

public:
    Permutation();

    Permutation(const Permutation &old);

    ~Permutation();

    void setLabelCallback(string (*newLabelFn) (T));

    void clear();

    void addInstruction(T inst);

    void addDependency(T a, T b);

    void addDirectDependency(T a, T b);

    int countPermutations();

    Schedule* getPermutation();

    Schedule* getPermutation(int permutation);

    list<T> getRandomPermutation();

    Schedule *permute(int *counter, int *stop, Schedule *schedule);
    
    bool scheduleInstruction(Schedule *schedule, T inst);

    void dumpGraph(string filename);

    void dumpDot(string filename, list<T> nodesToExclude);
};

#endif //LLVM_PERMUTATION_H
