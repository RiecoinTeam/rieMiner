VER    = 2610
CXX    = g++
CUDA   = /usr/local/cuda-13.4
NVCC   = $(CUDA)/bin/nvcc -arch=sm_80
M4     = m4
AS     = as
SED    = sed
CFLAGS = -Wall -Wextra -std=c++17 -O3 -fno-pie -no-pie
LIBS   = -lcurl -lgmpxx -lgmp

all: native

cuda: rieMinerCuda
cuda: CFLAGS += -DRIEMINERCUDA=1 -march=native -g

apple_silicon: CXX = clang++
apple_silicon: CFLAGS += -march=armv8.3-a -target arm64-apple-macos -O3 -I/opt/homebrew/include -I/opt/homebrew/opt/gmp/include
apple_silicon: LIBS := -L/opt/homebrew/opt/gmp/lib -lcurl -lgmpxx -lgmp
apple_silicon: rieMiner

native: CFLAGS += -march=native -s
native: LIBS := -pthread $(LIBS)
ifeq ($(shell uname -m),x86_64)
native: rieMinerx64
else
native: rieMiner
endif

debug: CFLAGS += -march=native -g
debug: LIBS := -pthread $(LIBS)
ifeq ($(shell uname -m),x86_64)
debug: rieMinerx64
else
debug: rieMiner
endif

Deb64: CFLAGS += -march=x86-64 -s -D CURL_STATICLIB -I incsDeb64/
Deb64AVX2: CFLAGS += -march=x86-64 -mavx2 -s -D CURL_STATICLIB -I incsDeb64/
Deb64 Deb64AVX2: LIBS := -Wl,-Bstatic -static-libstdc++ -L libsDeb64/ -pthread $(LIBS) -Wl,-Bdynamic
Deb64 Deb64AVX2: rieMinerx64
	mv rieMiner rieMiner$(VER)$@

Deb32: CXX = i686-linux-gnu-g++
Deb32: CFLAGS += -march=i686 -s -D CURL_STATICLIB -I incsDeb32/
Deb32: LIBS := -Wl,-Bstatic -static-libstdc++ -L libsDeb32/ -pthread $(LIBS) -Wl,-Bdynamic
Deb32: rieMiner
	mv rieMiner rieMiner$(VER)$@

Arm64: CXX = aarch64-linux-gnu-g++
Arm64: CFLAGS += -march=armv8-a -s -D CURL_STATICLIB -I incsArm64/
Arm64: LIBS := -Wl,-Bstatic -static-libstdc++ -L libsArm64/ -pthread $(LIBS) -Wl,-Bdynamic
Arm64 Arm32: rieMiner
	mv rieMiner rieMiner$(VER)$@

Arm32: CXX = arm-linux-gnueabihf-g++
Arm32: CFLAGS += -march=armv7 -s -D CURL_STATICLIB -I incsArm32/
Arm32: LIBS := -Wl,-Bstatic -static-libstdc++ -L libsArm32/ -pthread $(LIBS) -latomic -Wl,-Bdynamic

Win64 Win64AVX2: CXX = x86_64-w64-mingw32-g++-posix
Win64 Win64AVX2: AS = x86_64-w64-mingw32-as
Win64: CFLAGS += -march=x86-64 -s -D CURL_STATICLIB -I incsWin64/
Win64AVX2: CFLAGS += -march=x86-64 -mavx2 -s -D CURL_STATICLIB -I incsWin64/
Win64 Win64AVX2: LIBS := -Wl,-Bstatic -static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lpthread -Wl,--no-whole-archive -L libsWin64/ $(LIBS) -lws2_32 -lbcrypt -Wl,-Bdynamic -Wl,--image-base -Wl,0x10000000
Win64 Win64AVX2: rieMinerWin64
	mv rieMiner.exe rieMiner$(VER)$@.exe

Win32: CXX = i686-w64-mingw32-g++-posix
Win32: AS = i686-w64-mingw32-as
Win32: LIBS := -Wl,-Bstatic -static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lpthread -Wl,--no-whole-archive -L libsWin32/ $(LIBS) -lws2_32 -lbcrypt -Wl,-Bdynamic -Wl,--image-base -Wl,0x10000000
Win32: CFLAGS += -march=i686 -s -D CURL_STATICLIB -I incsWin32/
Win32: rieMiner
	mv rieMiner.exe rieMiner$(VER)$@.exe

And64: CXX = /home/user/dev/android-ndk-r25/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang++
And64: CFLAGS = -Wall -Wextra -std=c++17 -O3 -I incsAnd64/
And64: LIBS := -Wl,-Bstatic -static-libstdc++ -L libsAnd64/ -pthread $(LIBS) -Wl,-Bdynamic -Wl,--strip-all
And64 And32: rieMiner
	mv rieMiner rieMiner$(VER)$@

And32: CXX = /home/user/dev/android-ndk-r25/toolchains/llvm/prebuilt/linux-x86_64/bin/armv7a-linux-androideabi19-clang++
And32: CFLAGS = -Wall -Wextra -std=c++17 -O3 -fPIC -I incsAnd32/
And32: LIBS := -Wl,-Bstatic -static-libstdc++ -L libsAnd32/ -pthread $(LIBS) -Wl,-Bdynamic -Wl,--strip-all

testServer: rieMinerTestServer

rieMinerTestServer: TestServer.cpp
	$(CXX) -Wall -Wextra -std=c++20 $^ -o $@

rieMiner: main.o Stella.o StratumClient.o GBTClient.o Client.o API.o tools.o
	$(CXX) $(CFLAGS) -o rieMiner $^ $(LIBS)

rieMinerCuda: main.o StellaCuda.o StratumClient.o GBTClient.o Client.o API.o tools.o mod_1_4.o mod_1_2_avx.o mod_1_2_avx2.o fermatIspc.o fermatCuda.o primetest.o primetest512.o
	$(CXX) $(CFLAGS) -L $(CUDA)/lib64 -o rieMiner $^ $(LIBS) -lcuda -lcudart

rieMinerx64: main.o Stella.o StratumClient.o GBTClient.o Client.o API.o tools.o mod_1_4.o mod_1_2_avx.o mod_1_2_avx2.o fermatIspc.o primetest.o primetest512.o
	$(CXX) $(CFLAGS) -o rieMiner $^ $(LIBS)

rieMinerWin64: main.o Stella.o StratumClient.o GBTClient.o Client.o API.o tools.o mod_1_4_win.o mod_1_2_avx_win.o mod_1_2_avx2_win.o fermatIspc.o primetest_win.o primetest512_win.o
	$(CXX) $(CFLAGS) -o rieMiner $^ $(LIBS)

main.o: main.cpp main.hpp Stella.hpp Client.hpp tools.hpp
	$(CXX) $(CFLAGS) -c -o $@ -DversionShort=\"$(VER)\" -DversionString=\"rieMiner\ $(VER)\" main.cpp

Stella.o: Stella.cpp Stella.hpp
	$(CXX) $(CFLAGS) -c -o $@ Stella.cpp

StellaCuda.o: Stella.cpp Stella.hpp
	$(CXX) $(CFLAGS) -c -o $@ Stella.cpp

StratumClient.o: StratumClient.cpp
	$(CXX) $(CFLAGS) -c -o $@ StratumClient.cpp

GBTClient.o: GBTClient.cpp
	$(CXX) $(CFLAGS) -c -o $@ GBTClient.cpp

Client.o: Client.cpp
	$(CXX) $(CFLAGS) -c -o $@ Client.cpp

API.o: API.cpp
	$(CXX) $(CFLAGS) -c -o $@ API.cpp

tools.o: tools.cpp
	$(CXX) $(CFLAGS) -c -o $@ tools.cpp

fermatCuda.o: fermatCuda.cu
	$(NVCC) -I external/cgbn fermatCuda.cu -c fermatCuda.o -lgmpo

fermatIspc.o: ispc/fermat.cpp
	$(CXX) $(CFLAGS) -c -o $@ ispc/fermat.cpp -Wno-unused-function -Wno-unused-parameter -Wno-strict-overflow

mod_1_4_win.o: external/mod_1_4_win.asm
	$(M4) external/mod_1_4_win.asm >mod_1_4.s
	$(AS) mod_1_4.s -o $@
	rm mod_1_4.s

mod_1_4.o: external/mod_1_4.asm
	$(M4) external/mod_1_4.asm >mod_1_4.s
	$(AS) mod_1_4.s -o $@
	rm mod_1_4.s

mod_1_2_avx_win.o: external/mod_1_2_avx.asm external/mod_1_2_avx_win.sed
	$(SED) -f external/mod_1_2_avx_win.sed <external/mod_1_2_avx.asm >mod_1_2_avx.asm
	$(M4) mod_1_2_avx.asm >mod_1_2_avx.s
	$(AS) mod_1_2_avx.s -o $@
	rm mod_1_2_avx.s mod_1_2_avx.asm

mod_1_2_avx2_win.o: external/mod_1_2_avx2.asm external/mod_1_2_avx_win.sed
	$(SED) -f external/mod_1_2_avx_win.sed <external/mod_1_2_avx2.asm >mod_1_2_avx2.asm
	$(M4) mod_1_2_avx2.asm >mod_1_2_avx2.s
	$(AS) mod_1_2_avx2.s -o $@
	rm mod_1_2_avx2.s mod_1_2_avx2.asm

mod_1_2_avx.o: external/mod_1_2_avx.asm
	$(M4) external/mod_1_2_avx.asm >mod_1_2_avx.s
	$(AS) mod_1_2_avx.s -o $@
	rm mod_1_2_avx.s

mod_1_2_avx2.o: external/mod_1_2_avx2.asm
	$(M4) external/mod_1_2_avx2.asm >mod_1_2_avx2.s
	$(AS) mod_1_2_avx2.s -o $@
	rm mod_1_2_avx2.s

primetest_win.o: ispc/primetest.s ispc/primetest_win.sed
	$(SED) -f ispc/primetest_win.sed <ispc/primetest.s >primetest_win.s
	$(AS) primetest_win.s -o $@
	rm primetest_win.s

primetest512_win.o: ispc/primetest512.s ispc/primetest_win.sed
	$(SED) -f ispc/primetest_win.sed <ispc/primetest512.s >primetest512_win.s
	$(AS) primetest512_win.s -o $@
	rm primetest512_win.s

primetest.o: ispc/primetest.s
	$(AS) ispc/primetest.s -o $@

primetest512.o: ispc/primetest512.s
	$(AS) ispc/primetest512.s -o $@

clean:
	rm -rf *.o
