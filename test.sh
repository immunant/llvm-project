#-O2 required since stores are instrumented in dead register pass
#-mllvm to pass LLVM -debug arg
# ffixed-x18 required to emit extr with x18
./build/bin/clang -O2 -mllvm -debug \
    -march=armv8.5-a+memtag -ffixed-x18 \
    --target=aarch64-linux-gnu \
    -S $1 -o $1.s 2>&1 | tee log
cat $1.s
