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
#include <exception>
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

/// Helper function to detect the beginning of next game starting from a
/// random position inside the DB. Here is tricky because a part of game offset
/// could represent a MOVE_NONE value. We have to avoid to be fooled by this
/// alias.
Move* detect_next_game(Move* data) {

  // Forward scan to find the first game separator candidate
  while (*data != MOVE_NONE)
      data++;

  // This can be a real separator or a part of the game offset, because offset
  // is 4 moves long, we jump back of 4 moves and rescan. In case of a real
  // separator we end up at the same point, otherwise we stop earlier.
  data -= 4;
  while (*data != MOVE_NONE)
      data++;

  // FIXME handle the case of game shorter than 4 moves
  return data + 1;
}


/// search() re-play all the games and after each move look if the current
/// position matches the requested rules.

void search(Thread* th) {

  static_assert(sizeof(uint64_t) == 4 * sizeof(Move), "Wrong Move size");

  StateInfo states[1024], *st = states;
  size_t cnt = 0, matchCnt = 0, gameCnt = 0;
  uint64_t gameOfs;
  Scout::Data& d = th->scout;
  d.matches.reserve(100000);

  // Compute our file sub-range to search
  size_t range = d.dbSize  / Threads.size();
  Move* data = d.baseAddress + th->idx * range;
  Move* end = th->idx == Threads.size() - 1 ? d.baseAddress + d.dbSize
                                            : data + range;

  // Copy to locals hot-path variables
  const RuleType* rules = d.rules.data();
  Key matKey = d.matKey;
  const SubFen* subfens = d.subfens.data();
  const SubFen* subfensEnd = subfens + d.subfens.size();

  // Move to the beginning of the next game
  if (data != d.baseAddress)
  {
      assert(data > d.baseAddress + 4);

      data = detect_next_game(data);
  }

  data--; // Should point to MOVE_NONE (the end of previous game)

NextGame:

  assert(data < d.baseAddress || *data == MOVE_NONE);

  // Main loop, replay all games until we finish our file chunk
  while (++data < end)
  {
      // First 4 Moves store the game offset, skip them
      Move* gameOfsPtr = data;
      data += 4;

      assert(*data != MOVE_NONE);

      // First move stores the result
      GameResult result = GameResult(to_sq(Move(*data)));
      Position pos = th->rootPos;
      st = states;
      gameCnt = cnt;

      while (*++data != MOVE_NONE) // Could be an empty game
      {
          Move m = *data;

          assert(pos.pseudo_legal(m) && pos.legal(m));

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
                  goto NextMove;

              case RuleResult:
                  if (result != d.result)
                      goto SkipToNextGame;
                  break;

              case RuleSubFen:
                  for (const SubFen* f = subfens; f < subfensEnd; ++f)
                  {
                      if (   (pos.pieces(WHITE) & f->white) != f->white
                          || (pos.pieces(BLACK) & f->black) != f->black)
                          continue;

                      bool ok = true;
                      for (const auto& p : f->pieces)
                          if ((pos.pieces(p.first) & p.second) != p.second)
                          {
                              ok = false;
                              break;
                          }

                      if (ok)
                          goto success;
                  }
                  goto NextMove;
success:
                  break;

              case RuleMaterial:
                  if (pos.material_key() != matKey)
                      goto NextMove;
                  break;

              case RuleWhite:
                  if (pos.side_to_move() != WHITE)
                      goto NextMove;
                  break;

              case RuleBlack:
                  if (pos.side_to_move() != BLACK)
                      goto NextMove;
                  break;

              case RuleEnd:
                  matchCnt++; // All rules passed: success!
                  read_be(gameOfs, (uint8_t*)gameOfsPtr);
                  d.matches.push_back({gameOfs, uint16_t(cnt - gameCnt)});
SkipToNextGame:
                  // Skip to the end of the game after the first match
                  while (*++data != MOVE_NONE) { cnt++; }
                  goto NextGame;
              }
          }

NextMove: {}

      };
  }

  d.movesCnt = cnt;
  d.matchCnt = matchCnt;
}


/// print_results() collect info out of the threads at the end of the search
/// and print it out in JSON format.

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

  std::string tab = "\n    ";
  std::string indent4 = "    ";
  std::cout << "{"
            << tab << "\"moves\": " << cnt << ","
            << tab << "\"match count\": " << matches << ","
            << tab << "\"moves/second\": " << 1000 * cnt / elapsed << ","
            << tab << "\"processing time (ms)\": " << elapsed << ","
            << tab << "\"matches\":"
            << tab << "[";

  for (Thread* th : Threads)
      for (auto& m : th->scout.matches)
          std::cout << tab << indent4 << "{ \"ofs\": " << m.gameOfs
                                      << ", \"ply\": " << m.ply << "},";

  std::cout << tab << "]\n}" << std::endl;
}


/// parse_rules() read a JSON input, extract the requested rules and fill the
/// Scout::Data struct to be used during the search.

void parse_rules(Scout::Data& data, std::istringstream& is) {

  /* Examples of JSON queries:

      { "sub-fen": "8/8/p7/8/8/1B3N2/8/8" }
      { "sub-fen": "8/8/8/8/1k6/8/8/8", "material": "KBNKP" }
      { "material": "KBNKP", "stm": "WHITE" }
      { "material": "KNNK", "result": "1-0" }
      { "sub-fen": ["8/8/8/q7/8/8/8/8", "8/8/8/r7/8/8/8/8"] }

  */

  json j = json::parse(is);

  if (!j["result"].empty())
  {
      GameResult result =  j["result"] == "1-0" ? WhiteWin
                         : j["result"] == "0-1" ? BlackWin
                         : j["result"] == "1/2-1/2" ? Draw
                         : j["result"] == "*" ? Unknown : Invalid;

      if (result != Invalid)
      {
          data.result = result;
          data.rules.push_back(RuleResult);
      }
  }

  if (!j["sub-fen"].empty())
  {
      for (const auto& fen : j["sub-fen"])
      {
          StateInfo st;
          Position pos;
          pos.set(fen, false, &st, nullptr, true);

          // Setup the pattern to be searched
          SubFen f;
          f.white = pos.pieces(WHITE);
          f.black = pos.pieces(BLACK);
          for (PieceType pt = PAWN; pt <= KING; ++pt)
              if (pos.pieces(pt))
                  f.pieces.push_back(std::make_pair(pt, pos.pieces(pt)));

          data.subfens.push_back(f);
      }
      data.rules.push_back(Scout::RuleSubFen);
  }

  if (!j["material"].empty())
  {
      StateInfo st;
      data.matKey = Position().set(j["material"], WHITE, &st).material_key();
      data.rules.push_back(Scout::RuleMaterial);
  }

  if (!j["stm"].empty())
  {
      auto rule = j["stm"] == "WHITE" ? Scout::RuleWhite : Scout::RuleBlack;
      data.rules.push_back(rule);
  }

  data.rules.push_back(data.rules.size() ? Scout::RuleEnd : Scout::RuleNone);
}

} // namespace Scout
