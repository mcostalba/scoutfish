/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2016 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <sstream>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#else
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "misc.h"
#include "position.h"
#include "thread.h"
#include "uci.h"

namespace {

void map(const char* fname, void** baseAddress, uint64_t* mapping, size_t* size) {

#ifndef _WIN32
    struct stat statbuf;
    int fd = ::open(fname, O_RDONLY);
    fstat(fd, &statbuf);
    *mapping = *size = statbuf.st_size;
    *baseAddress = mmap(nullptr, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
    ::close(fd);
    if (*baseAddress == MAP_FAILED)
    {
        std::cerr << "Could not mmap() " << fname << std::endl;
        exit(1);
    }
#else
    HANDLE fd = CreateFile(fname, GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD size_high;
    DWORD size_low = GetFileSize(fd, &size_high);
    HANDLE mmap = CreateFileMapping(fd, nullptr, PAGE_READONLY, size_high, size_low, nullptr);
    CloseHandle(fd);
    if (!mmap)
    {
        std::cerr << "CreateFileMapping() failed" << std::endl;
        exit(1);
    }
    *size = ((size_t)size_high << 32) | (size_t)size_low;
    *mapping = (uint64_t)mmap;
    *baseAddress = MapViewOfFile(mmap, FILE_MAP_READ, 0, 0, 0);
    if (!*baseAddress)
    {
        std::cerr << "MapViewOfFile() failed, name = " << fname
                  << ", error = " << GetLastError() << std::endl;
        exit(1);
    }
#endif
}

void unmap(void* baseAddress, uint64_t mapping) {

#ifndef _WIN32
    munmap(baseAddress, mapping);
#else
    UnmapViewOfFile(baseAddress);
    CloseHandle((HANDLE)mapping);
#endif
}

size_t do_scout(const Position& startPos, Move* data, uint64_t size) {

    StateInfo states[1024], *st = states;
    size_t cnt = 0;
    Move* eof = data + size / sizeof(Move);

    // Look for beginning of next game
    while (*data++ != MOVE_NONE) {}

    // Main loop
    while (data < eof)
    {
        assert(*data != MOVE_NONE);

        Position pos = startPos;
        st = states;

        do {
            Move m = *data;

            assert(pos.legal(m));

            pos.do_move(m, *st++, pos.gives_check(m));
            cnt++;

        } while (*++data != MOVE_NONE);

        // End of game, move to next
        while (*++data == MOVE_NONE && data < eof) {}
    }

    return cnt;
}

} // namespace


void scout(const Position& startPos, std::istringstream& is) {

    uint64_t mapping, size;
    void* baseAddress;
    std::string dbName;

    is >> dbName;

    if (dbName.empty())
    {
        std::cerr << "Missing PGN file name..." << std::endl;
        exit(0);
    }

    map(dbName.c_str(), &baseAddress, &mapping, &size);

    TimePoint elapsed = now();

    size_t cnt = do_scout(startPos, (Move*)baseAddress, size);

    elapsed = now() - elapsed + 1; // Ensure positivity to avoid a 'divide by zero'

    unmap(baseAddress, mapping);

    std::cerr << "\nMoves: " << cnt
              << "\nMoves/second: " << 1000 * cnt / elapsed
              << "\nProcessing time (ms): " << elapsed << "\n" << std::endl;
}
