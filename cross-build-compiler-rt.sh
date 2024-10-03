#!/bin/sh
# see https://llvm.org/docs/HowToCrossCompileBuiltinsOnArm.html
mkdir -p build-compiler-rt
cd build-compiler-rt
cross_flags="--gcc-toolchain=/usr -isystem /usr/aarch64-linux-gnu/include -march=armv8.5-a+memtag -ffixed-x18"
export LDFLAGS="-L/usr/aarch64-linux-gnu/lib"
cmake -GNinja -DLLVM_TARGETS_TO_BUILD="AArch64" -DLLVM_DEFAULT_TARGET_TRIPLE="aarch64-linux-gnu" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_FLAGS="$cross_flags --target=aarch64-linux-gnu" -DCMAKE_CXX_FLAGS="$cross_flags --target=aarch64-linux-gnu" \
    -DCMAKE_C_COMPILER="$(pwd)/../build/bin/clang" -DCMAKE_CXX_COMPILER="$(pwd)/../build/bin/clang" \
    -DCMAKE_BUILD_WITH_INSTALL_RPATH=true \
    -DCOMPILER_RT_BUILD_SANITIZERS=OFF -DCOMPILER_RT_BUILD_MEMPROF=OFF -DCOMPILER_RT_BUILD_ORC=OFF \
    -DCOMPILER_RT_BUILD_XRAY=OFF -DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
    -DCOMPILER_RT_DEFAULT_TARGET_TRIPLE="aarch64-linux-gnu" \
    ../compiler-rt
ninja
cd lib/linux
# rename CRT files to expected filenames
cp -a clang_rt.crtend-aarch64.o crtendS.o
cp -a clang_rt.crtbegin-aarch64.o crtbeginS.o
