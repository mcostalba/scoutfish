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

#include "misc.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "uci.h"

namespace Scout {

void search(const Search::LimitsType& limits, Thread* th) {

  StateInfo states[1024], *st = states;
  size_t cnt = 0;

  // Compute our file sub-range to search
  size_t range = limits.dbSize  / Threads.size();
  Move* data = limits.baseAddress + th->idx * range;
  Move* end = th->idx == Threads.size() - 1 ? limits.baseAddress + limits.dbSize
                                            : data + range;
  // Move to the beginning of the next game
  while (*data++ != MOVE_NONE) {}

  // Main loop, replay all games until we finish our file chunk
  while (data < end)
  {
      assert(*data != MOVE_NONE);

      Position pos = th->rootPos;
      st = states;

      do {
          Move m = *data;

          assert(pos.legal(m));

          pos.do_move(m, *st++, pos.gives_check(m));
          cnt++;

      } while (*++data != MOVE_NONE);

      // End of game, move to next
      while (*++data == MOVE_NONE && data < end) {}
  }

  th->scoutResults.movesCnt = cnt;
}

void print_results(const Search::LimitsType& limits) {

  TimePoint elapsed = now() - limits.startTime + 1;
  size_t cnt = 0;

  mem_unmap(limits.baseAddress, limits.dbMapping);

  for (Thread* th : Threads)
      cnt += th->scoutResults.movesCnt;

  std::cerr << "\nMoves: " << cnt
            << "\nMoves/second: " << 1000 * cnt / elapsed
            << "\nProcessing time (ms): " << elapsed << "\n" << std::endl;
}

} // namespace
