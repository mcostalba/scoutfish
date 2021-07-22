# Build

The [Makefile](src/Makefile) is used to build the application,
which should work on most of the architectures easily.


A typical command to build it on MacOSX for M1 is as follows:

```
make ARCH=aarch64 COMP=clang COMPCXX=clang++
```
