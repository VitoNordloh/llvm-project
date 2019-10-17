add_llvm_loadable_module(LLVMProfileReader
    ProfileReader.cpp

    ADDITIONAL_HEADER_DIRS
    ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/ProfileReader
)