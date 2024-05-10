mkdir -p build
cd build
cmake -GNinja -DLLVM_ENABLE_PROJECTS=clang \
    -DLLVM_TARGETS_TO_BUILD="AArch64" \
    -DCMAKE_BUILD_TYPE=Debug \
    ../llvm
ninja clang
