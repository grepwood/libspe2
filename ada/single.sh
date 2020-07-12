spu-gcc helloworld.c -o hello
ppu-gcc -c libspe2.ads libspe2_types.ads
ppu-gnatmake testsingle.adb -o testsingle -largs -lspe2
