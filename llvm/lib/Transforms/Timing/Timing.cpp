#include <ctime>

double cps = CLOCKS_PER_SEC;

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

using namespace std;
using namespace llvm;

struct Timing : public ModulePass {
    static char ID;
    Timing() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {
        // Create clock function
        Type* clock_t = Type::getInt64Ty(M.getContext());
        FunctionType* clockTy = FunctionType::get(clock_t, false);
        Value* clock = M.getOrInsertFunction("clock", clockTy);

        // Create printf function
        Type* printf_t = Type::getVoidTy(M.getContext());
        vector<Type*> params;
        params.push_back(Type::getInt8PtrTy(M.getContext()));
        FunctionType* printfTy = FunctionType::get(printf_t, params, true);
        Value* printf = M.getOrInsertFunction("printf", printfTy);

        for(auto& F : M) {
            if(F.getName() == "main") {
                IRBuilder<> builder(M.getContext());

                // Insert first call to clock
                Instruction* firstInstruction = F.getEntryBlock().getFirstNonPHIOrDbg();
                builder.SetInsertPoint(firstInstruction);
                CallInst* start = builder.CreateCall(clock);

                for(auto& BB : F) {
                    for(auto& I : BB) {
                        bool insert = false;

                        // Look for ret instruction and calls to exit
                        if(strcmp(I.getOpcodeName(), "ret") == 0) {
                            insert = true;
                        } else if(strcmp(I.getOpcodeName(), "call") == 0) {
                            CallInst* callInst = cast<CallInst>(&I);
                            Function* calledFunction = callInst->getCalledFunction();
                            if(calledFunction != 0 && calledFunction->getName().equals("exit")) {
                                insert = true;
                            }
                        }

                        // Insert call
                        if(insert) {
                            builder.SetInsertPoint(&I);
                            CallInst *end = builder.CreateCall(clock);

                            // Calculate time
                            Value *diff = builder.CreateSub(end, start);
                            Value *castedDiff = builder.CreateUIToFP(diff, Type::getDoubleTy(M.getContext()));
                            Constant *clocksPerSec = ConstantFP::get(Type::getDoubleTy(M.getContext()), cps);
                            Value *div = builder.CreateFDiv(castedDiff, clocksPerSec);

                            // Print time
                            Constant *format = builder.CreateGlobalStringPtr("Total Time: %f");
                            vector < Value * > p;
                            p.push_back(format);
                            p.push_back(div);
                            builder.CreateCall(printf, p);
                        }
                    }
                }
            }
        }
        return true;
    }
};

char Timing::ID = 0;
static RegisterPass<Timing> X("timing", "Timing Code insertion pass", false, false);