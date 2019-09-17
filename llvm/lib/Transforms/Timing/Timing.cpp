#define DEBUG_TYPE "timing-pass"

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
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/APInt.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

#include <vector>
#include <time.h>
#include <string>

using namespace std;
using namespace llvm;

#ifdef __APPLE__
string clockName("CLOCK_REALTIME");
clockid_t clock_id = CLOCK_REALTIME;
#else
string clockName("CLOCK_MONOTONIC");
clockid_t clock_id = CLOCK_MONOTONIC;
#endif

struct Timing : public ModulePass {
    static char ID;
    Timing() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {
        LLVM_DEBUG(dbgs() << "Using " << clockName << "\n");

        // Create timespec struct
        vector<Type*> elements;
        elements.push_back(Type::getInt64Ty(M.getContext()));
        elements.push_back(Type::getInt64Ty(M.getContext()));
        StructType* timespec = StructType::create(M.getContext(), elements);

        // Create clock function
        Type* clock_t = Type::getInt32Ty(M.getContext());

        vector<Type*> clockParams;
        clockParams.push_back(Type::getInt32Ty(M.getContext()));
        clockParams.push_back(timespec->getPointerTo());

        FunctionType* clockTy = FunctionType::get(clock_t, clockParams, false);
        Value* clock = M.getOrInsertFunction("clock_gettime", clockTy);

        // Create printf function
        Type* printf_t = Type::getVoidTy(M.getContext());
        vector<Type*> params;
        params.push_back(Type::getInt8PtrTy(M.getContext()));
        FunctionType* printfTy = FunctionType::get(printf_t, params, true);
        Value* printf = M.getOrInsertFunction("printf", printfTy);

        for(auto& F : M) {
            if(F.getName() == "main") {
                IRBuilder<> builder(M.getContext());
                Instruction* firstInstruction = F.getEntryBlock().getFirstNonPHIOrDbg();
                builder.SetInsertPoint(firstInstruction);

                // Create structs for start and end
                AllocaInst* allocStart = builder.CreateAlloca(timespec);
                AllocaInst* allocEnd = builder.CreateAlloca(timespec);

                // Insert first call to clock_gettime
                vector<Value*> paramsFirstCall;
                paramsFirstCall.push_back(ConstantInt::get(Type::getInt32Ty(M.getContext()), clock_id));
                paramsFirstCall.push_back(allocStart);
                CallInst* start = builder.CreateCall(clock, paramsFirstCall);

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

                            // Insert second call
                            vector<Value*> paramsSecondCall;
                            paramsSecondCall.push_back(ConstantInt::get(Type::getInt32Ty(M.getContext()), clock_id));
                            paramsSecondCall.push_back(allocEnd);
                            CallInst* end = builder.CreateCall(clock, paramsSecondCall);

                            // Calculate time
                            Value *startPtrS = builder.CreateStructGEP(timespec, allocStart, 0);
                            Value *startTimeS = builder.CreateLoad(startPtrS);
                            Value *startPtrNS = builder.CreateStructGEP(timespec, allocStart, 1);
                            Value *startTimeNS = builder.CreateLoad(startPtrNS);

                            Value *endPtrS = builder.CreateStructGEP(timespec, allocEnd, 0);
                            Value *endTimeS = builder.CreateLoad(endPtrS);
                            Value *endPtrNS = builder.CreateStructGEP(timespec, allocEnd, 1);
                            Value *endTimeNS = builder.CreateLoad(endPtrNS);

                            Value *diffS = builder.CreateSub(endTimeS, startTimeS);
                            Value *diffNS = builder.CreateSub(endTimeNS, startTimeNS);

                            Value *castedDiffS = builder.CreateSIToFP(diffS, Type::getDoubleTy(M.getContext()));
                            Value *castedDiffNS = builder.CreateSIToFP(diffNS, Type::getDoubleTy(M.getContext()));

                            Constant *nsPerSec = ConstantFP::get(Type::getDoubleTy(M.getContext()), 1E9);
                            Value *div = builder.CreateFDiv(castedDiffNS, nsPerSec);

                            Value *final = builder.CreateFAdd(castedDiffS, div);

                            // Print time
                            Constant *format = builder.CreateGlobalStringPtr("Total Time: %f");
                            vector<Value*> p;
                            p.push_back(format);
                            p.push_back(final);
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