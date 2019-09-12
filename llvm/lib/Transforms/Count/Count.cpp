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
#include "llvm/Support/Casting.h"
#include "llvm/ADT/APInt.h"

#include <string>

using namespace llvm;
using namespace std;

namespace {
    struct Count : public ModulePass {
        static char ID;
        Count() : ModulePass(ID) {}

        bool runOnModule(Module &M) override {
            errs() << "Module found" << '\n';

            // Void type
            Type* voidTy = Type::getVoidTy(M.getContext());

            // Create initialize_profiler function
            FunctionType* initType = FunctionType::get(voidTy, false);
            Value* initFn = M.getOrInsertFunction("initialize_profiler", initType);

            // Create increment_counter function
            std::vector<Type*> incArgs;
            PointerType* cStr = PointerType::getInt8PtrTy(M.getContext());
            incArgs.push_back(cStr);
            FunctionType* incType = FunctionType::get(voidTy, incArgs, false);
            Value* incFn = M.getOrInsertFunction("increment_counter", incType);

            // Create finalize_profiler function
            FunctionType* finType = FunctionType::get(voidTy, false);
            Value* finFn = M.getOrInsertFunction("finalize_profiler", initType);

            for(auto F = M.begin(); F != M.end(); F++) {
                errs() << "  Function found: " << F->getName() << '\n';

                int i = 0;
                for(auto BB = F->getBasicBlockList().begin(); BB != F->getBasicBlockList().end(); BB++) {
                    errs() << "    BasicBlock found: " << BB->getName() << '\n';

                    IRBuilder<> Builder(BB->getFirstNonPHI());

                    // Create list of arguments and insert call
                    string name;
                    name += F->getName();
                    name += "_";
                    name += BB->getName();
                    Constant* string = Builder.CreateGlobalStringPtr(name.c_str());
                    std::vector<Value*> args;
                    args.push_back(string);
                    Builder.CreateCall(incFn, args);
                    errs() << "    Increment call inserted" << '\n';

                    // Look for 'ret' instruction in main
                    if(F->getName().equals("main")) {
                        Instruction* I = BB->getTerminator();
                        if(strcmp(I->getOpcodeName(), "ret") == 0) {
                            Builder.SetInsertPoint(I);
                            Builder.CreateCall(finFn);
                            errs() << "    Finalize call inserted before return" << '\n';
                        }
                    }

                    // Look for call to exit
                    for(auto I = BB->begin(); I != BB->end(); I++) {
                        if(strcmp(I->getOpcodeName(), "call") == 0) {
                            errs() << "    call function found in main" << '\n';
                            CallInst* callInst = cast<CallInst>(I);
                            Function* calledFunction = callInst->getCalledFunction();
                            if(calledFunction != 0 && calledFunction->getName().equals("exit")) {
                                Builder.SetInsertPoint(cast<Instruction>(I));
                                Builder.CreateCall(finFn);
                                errs() << "    Finalize call inserted before exit" << '\n';
                            }
                        }
                    }

                    i++;
                }
            }
            return true;
        }
    };
}

char Count::ID = 0;
static RegisterPass<Count> X("count", "Count Pass", false, false);