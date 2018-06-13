# kscope
kaleidoscope language/compiler from llvm tutorial

## Compilation
```
clang++ --std=c++17 -I/usr/local/Cellar/llvm/6.0.0/include/ `llvm-config --ldflags --system-libs --libs core` -o kc driver.cpp
```
