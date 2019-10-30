add_llvm_loadable_module(LLVMStats
    Stats.cpp

    ADDITIONAL_HEADER_DIRS
    ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/Stats
)