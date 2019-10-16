add_llvm_library(LLVMPermutation
    Permutation.cpp

    ADDITIONAL_HEADER_DIRS
    ${LLVM_MAIN_INCLUDE_DIR}/llvm/Permutation
)
