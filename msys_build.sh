#!/bin/sh

export PATH=.:/mingw64/bin:/usr/local/bin:/mingw/bin:/bin
cd src

make clean
make profile-build COMP=mingw ARCH=x86-64
strip stockfish.exe
mv stockfish.exe ../stockfish-windows-amd64.exe

make clean
make profile-build COMP=mingw ARCH=x86-64-modern
strip stockfish.exe
mv stockfish.exe ../stockfish-windows-amd64-modern.exe

make clean
make profile-build COMP=mingw ARCH=x86-64-bmi2
strip stockfish.exe
mv stockfish.exe ../stockfish-windows-amd64-bmi2.exe

cd ..
ls -lh
