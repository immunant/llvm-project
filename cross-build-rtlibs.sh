#!/bin/sh
# build compiler-rt, libcxx, libcxxabi, and libunwind with our patched clang
# see https://llvm.org/docs/HowToCrossCompileBuiltinsOnArm.html
set -x

# build stub crt object files to prevent wrong-arch ones from being selected
fake_crt_dir=$(pwd)/fake_crt
mkdir -p $fake_crt_dir
for crtobj in crtn.o crti.o Scrt1.o; do
	aarch64-linux-gnu-gcc -c -x c /dev/null -o $fake_crt_dir/$crtobj
done

# build runtime libs
mkdir -p build-rtlibs
cd build-rtlibs

# find existing aarch64 crt for building test programs
crt_candidates="$(ls -d /usr/lib/gcc/aarch64-linux-gnu/*)
$(ls -d /usr/lib/gcc-cross/aarch64-linux-gnu/*)"
crt_dir=$(echo $crt_candidates | sort -V | tail -n1)

cross_flags="-B$fake_crt_dir -B$crt_dir --gcc-toolchain=/usr --gcc-triple=aarch64-linux-gnu -isystem /usr/aarch64-linux-gnu/include -march=armv8+memtag -ffixed-x18"
#--sysroot=/usr/aarch64-linux-gnu/ --gcc-install-dir=/usr/lib/gcc/aarch64-linux-gnu/14.1.0
export LDFLAGS="-L/usr/aarch64-linux-gnu/lib"
cmake -GNinja -DLLVM_TARGETS_TO_BUILD="AArch64" -DLLVM_DEFAULT_TARGET_TRIPLE="aarch64-linux-gnu" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_FLAGS="$cross_flags --target=aarch64-linux-gnu" -DCMAKE_CXX_FLAGS="$cross_flags --target=aarch64-linux-gnu" \
    -DCMAKE_C_COMPILER="$(pwd)/../build/bin/clang" -DCMAKE_CXX_COMPILER="$(pwd)/../build/bin/clang" \
    -DCMAKE_BUILD_WITH_INSTALL_RPATH=true \
    -DCOMPILER_RT_BUILD_BUILTINS=ON \
    -DCOMPILER_RT_USE_BUILTINS_LIBRARY=ON \
    -DCOMPILER_RT_BUILD_SANITIZERS=OFF -DCOMPILER_RT_BUILD_MEMPROF=OFF -DCOMPILER_RT_BUILD_ORC=OFF -DCOMPILER_RT_BUILD_XRAY=OFF -DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
    -DLIBCXX_USE_COMPILER_RT=YES \
    -DLLVM_ENABLE_RUNTIMES='libcxx;libcxxabi;compiler-rt;libunwind' \
    ../runtimes
ninja
cd compiler-rt/lib/linux
# rename CRT files to expected filenames
cp -a clang_rt.crtend-aarch64.o crtendS.o
cp -a clang_rt.crtbegin-aarch64.o crtbeginS.o
