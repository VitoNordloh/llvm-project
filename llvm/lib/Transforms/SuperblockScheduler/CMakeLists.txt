add_llvm_loadable_module(LLVMSuperblockScheduler
    SuperblockSchedulerPass.cpp

    ADDITIONAL_HEADER_DIRS
    ${LLVM_MAIN_INCLUDE_DIR}/llvm/Permutation
)