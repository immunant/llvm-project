#!/bin/sh
# build compiler-rt, libcxx, libcxxabi, and libunwind with our patched clang
# see https://llvm.org/docs/HowToCrossCompileBuiltinsOnArm.html
set -x
mkdir -p build-rtlibs
cd build-rtlibs
ls -l /usr/lib/gcc/aarch64-linux-gnu
find /usr/lib | fgrep crtbeginS.o
crt_candidates="$(ls -d /usr/lib/gcc/aarch64-linux-gnu/*)
$(ls -d /usr/lib/gcc-cross/aarch64-linux-gnu/*)"
crt_dir=$(echo $crt_candidates | sort -V | tail -n1)

cross_flags="-B$crt_dir --sysroot=/usr/aarch64-linux-gnu --gcc-install-dir=$crt_dir -isystem /usr/aarch64-linux-gnu/include --rtlib=compiler-rt -march=armv8+memtag -ffixed-x18"
#--sysroot=/usr/aarch64-linux-gnu/ --gcc-install-dir=/usr/lib/gcc/aarch64-linux-gnu/14.1.0
export LDFLAGS="-L/usr/aarch64-linux-gnu/lib"
cmake -GNinja -DLLVM_TARGETS_TO_BUILD="AArch64" -DLLVM_DEFAULT_TARGET_TRIPLE="aarch64-linux-gnu" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_FLAGS="$cross_flags --target=aarch64-linux-gnu" -DCMAKE_CXX_FLAGS="$cross_flags --target=aarch64-linux-gnu" \
    -DCMAKE_C_COMPILER="$(pwd)/../build/bin/clang" -DCMAKE_CXX_COMPILER="$(pwd)/../build/bin/clang" \
    -DCMAKE_BUILD_WITH_INSTALL_RPATH=true \
    -DCMAKE_EXE_LINKER_FLAGS='--rtlib=compiler-rt' \
    -DCOMPILER_RT_BUILD_BUILTINS=ON \
    -DLIBCXX_USE_COMPILER_RT=YES \
    -DLLVM_ENABLE_RUNTIMES='libcxx;libcxxabi;compiler-rt;libunwind' \
    ../runtimes
ninja
cd compiler-rt/lib/linux
# rename CRT files to expected filenames
cp -a clang_rt.crtend-aarch64.o crtendS.o
cp -a clang_rt.crtbegin-aarch64.o crtbeginS.o
