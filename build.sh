# May need to set -DLLVM_TARGETS_TO_BUILD="AArch64;X86" for CMake to build some
# of the test programs it compiles at config-time. This may also require setting
# the default target triple to x86_64-linux-gnu
mkdir -p build
cd build
export CFLAGS="-ffixed-x18"
export CXXFLAGS="-ffixed-x18"
cmake -GNinja -DLLVM_ENABLE_PROJECTS="clang" \
    -DLLVM_TARGETS_TO_BUILD="AArch64" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCOMPILER_RT_BUILD_BUILTINS=ON \
    -DLLVM_ENABLE_RUNTIMES="compiler-rt" \
    -DLLVM_DEFAULT_TARGET_TRIPLE="aarch64-linux-gnu" \
    ../llvm
ninja clang builtins
# copy to expected target triple
cp -arv lib/clang/19/lib/aarch64-linux-gnu lib/clang/19/lib/aarch64-unknown-linux-gnu
