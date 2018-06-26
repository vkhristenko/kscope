binary=kint

object:
	clang++ --std=c++17 -I/usr/local/Cellar/llvm/6.0.0/include/ `llvm-config --ldflags --system-libs --libs all` -o $(binary) driver_object.cpp

jit: 
	clang++ --std=c++17 -DKINIT_JIT -I/usr/local/Cellar/llvm/6.0.0/include/ `llvm-config --ldflags --system-libs --libs core mcjit native` -o $(binary) driver.cpp

debug:
	clang++ --std=c++17 -DKINIT_DEBUG -DKINIT_JIT -I/usr/local/Cellar/llvm/6.0.0/include/ `llvm-config --ldflags --system-libs --libs core mcjit native` -o $(binary) driver.cpp

clean:
	rm -f $(binary)
