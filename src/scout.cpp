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

#include "json.hpp"
#include "misc.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "uci.h"

using json = nlohmann::json;

namespace Scout {

/// search() re-play all the games and after each move look if the current
/// position matches the requested rules.

void search(Thread* th) {

  StateInfo states[1024], *st = states;
  size_t cnt = 0, matchCnt = 0;
  Scout::Data& d = th->scout;

  // Compute our file sub-range to search
  size_t range = d.dbSize  / Threads.size();
  Move* data = d.baseAddress + th->idx * range;
  Move* end = th->idx == Threads.size() - 1 ? d.baseAddress + d.dbSize
                                            : data + range;

  // Copy to locals hot-path variables
  const RuleType* rules = d.rules;
  Bitboard all = d.pattern.all;
  Bitboard white = d.pattern.white;
  const auto& pieces = d.pattern.pieces;

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
          const RuleType* curRule = rules - 1;

          // Loop across matching rules, early exit as soon as
          // a match fails.
          while (true)
          {
              switch (*++curRule)
              {
              case RuleNone:
                  goto EndWhile;

              case RulePattern:
                  if (   (pos.pieces() & all) != all
                      || (pos.pieces(WHITE) & white) != white)
                      goto EndWhile;

                  for (const auto& p : pieces)
                     if ((pos.pieces(p.first) & p.second) != p.second)
                         goto EndWhile;

                  // Matched! Let's continue with next rule
                  break;

              case RuleEnd:
                  matchCnt++; // All rules passed: success!
                  goto EndWhile;
              }
          }

EndWhile: {}

      } while (*++data != MOVE_NONE);

      // End of game, move to next
      while (*++data == MOVE_NONE && data < end) {}
  }

  d.movesCnt = cnt;
  d.matchCnt = matchCnt;
}


/// print_results() collect info out of the threads at the end of the search
/// and print it out.

void print_results(const Search::LimitsType& limits) {

  TimePoint elapsed = now() - limits.startTime + 1;
  Scout::Data d = Threads.main()->scout;
  size_t cnt = 0, matches = 0;

  mem_unmap(d.baseAddress, d.dbMapping);

  for (Thread* th : Threads)
  {
      cnt += th->scout.movesCnt;
      matches += th->scout.matchCnt;
  }

  std::cerr << "\nMoves: " << cnt
            << "\nMatches found: " << matches
            << "\nMoves/second: " << 1000 * cnt / elapsed
            << "\nProcessing time (ms): " << elapsed << "\n" << std::endl;
}


/// parse_rules() read a JSON input, extract the requested rules and fill the
/// Scout::Data struct to be used during the search.

void parse_rules(Scout::Data& d, const std::string& jsonStr) {

  //std::string dbg = "{ \"fen\": \"8/8/p7/8/8/1B3N2/8/8\" }";

  json j = json::parse(jsonStr);

  if (!j["fen"].empty())
  {
      StateInfo st;
      Position pos;
      pos.set(j["fen"], false, &st, nullptr, true);

      // Setup the pattern to be searched
      auto& p = d.pattern;
      p.all = pos.pieces();
      p.white = pos.pieces(WHITE);
      for (PieceType pt = PAWN; pt <= KING; ++pt)
          if (pos.pieces(pt))
              p.pieces.push_back(std::make_pair(pt, pos.pieces(pt)));

      d.rules[0] = Scout::RulePattern;
      d.rules[1] = Scout::RuleEnd;
  }

}

} // namespace
