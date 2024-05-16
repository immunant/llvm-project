# May need to set -DLLVM_TARGETS_TO_BUILD="AArch64;X86" for CMake to build some
# of the test programs it compiles at config-time. This may also require setting
# the default target triple to x86_64-linux-gnu
mkdir -p build
cd build
cmake -GNinja -DLLVM_ENABLE_PROJECTS="clang" \
    -DLLVM_TARGETS_TO_BUILD="AArch64" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DLLVM_DEFAULT_TARGET_TRIPLE="aarch64-linux-gnu" \
    ../llvm
ninja clang
