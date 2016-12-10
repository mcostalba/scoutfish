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
  uint64_t gameOfs;
  const Condition* cond;
  const RuleType *rules, *curRule;
  const SubFen *subfens, *subfensEnd;
  std::vector<size_t> matchPlies;
  size_t condIdx = 0, streakStartPly = 0;
  Scout::Data& d = th->scout;
  d.matches.reserve(100000);
  matchPlies.reserve(128);

  // Lambda helper to copy hot stuff to local variables
  auto set_condition = [&] (size_t idx) {
    cond = d.conditions.data() + idx;
    rules = cond->rules.data();
    subfens = cond->subfens.data();
    subfensEnd = subfens + cond->subfens.size();

    // When starting a new streak ignore previous plies
    if (   idx
        && cond->streakId
        && cond->streakId != (cond-1)->streakId)
        streakStartPly = matchPlies.size();
  };
  set_condition(condIdx);

  // Compute our file sub-range to search
  size_t range = d.dbSize  / Threads.size();
  Move* data = d.baseAddress + th->idx * range;
  Move* end = th->idx == Threads.size() - 1 ? d.baseAddress + d.dbSize : data + range;

  // Move to the beginning of the next game
  if (data != d.baseAddress)
  {
      assert(data > d.baseAddress + 4);

      data = detect_next_game(data);
  }

  data--; // Should point to MOVE_NONE (the end of previous game)

  assert(data < d.baseAddress || *data == MOVE_NONE);

  // Main loop, replay all games until we finish our file chunk
  while (++data < end)
  {
      // First 4 Moves store the game offset, skip them
      Move* gameOfsPtr = data;
      data += 4;

      // First move stores the result in the 'to' square
      GameResult result = GameResult(to_sq(Move(*data)));
      Position pos = th->rootPos;
      st = states;

      // Reset to first condition if needed: FIXME may be useless, see bottom one
      if (condIdx != 0)
      {
          condIdx = 0;
          set_condition(condIdx);
          matchPlies.clear();
      }

      // Loop across the game (that could be empty)
      while (*++data != MOVE_NONE)
      {
          Move m = *data;

          assert(pos.pseudo_legal(m) && pos.legal(m));

          pos.do_move(m, *st++, pos.gives_check(m));
          curRule = rules;

          // If we are looking for a streak, reset in case last match
          // is more than one move behind.
          if (   cond->streakId
              && matchPlies.size() - streakStartPly > 0
              && matchPlies.back() != pos.nodes_searched() - 1)
          {
              assert(condIdx);

              condIdx = 0;
              set_condition(condIdx);
              matchPlies.clear();
          }

NextRule: // Loop across rules, early exit as soon as a match fails
          switch (*curRule++) {

          case RuleNone:
              break;

          case RuleResult:
              if (result == cond->result)
                  goto NextRule;
              goto SkipToNextGame; // Shortcut: result will not change

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
                      goto NextRule;
              }
              break;

          case RuleMaterial:
              if (std::find(cond->matKeys.begin(), cond->matKeys.end(),
                            pos.material_key()) != cond->matKeys.end())
                  goto NextRule;
              break;

          case RuleWhite:
              if (pos.side_to_move() == WHITE)
                  goto NextRule;
              break;

          case RuleBlack:
              if (pos.side_to_move() == BLACK)
                  goto NextRule;
              break;

          case RuleMatchedCondition:
              assert(condIdx + 1 < d.conditions.size());

              matchPlies.push_back(pos.nodes_searched());
              set_condition(++condIdx);
              break; // Skip to next move

          case RuleMatchedQuery:
              assert(condIdx + 1 == d.conditions.size());

              matchPlies.push_back(pos.nodes_searched());
              read_be(gameOfs, (uint8_t*)gameOfsPtr);
              d.matches.push_back({gameOfs, matchPlies});
              matchPlies.clear();
SkipToNextGame:
              // Skip to the end of the game after the first match
              while (*++data != MOVE_NONE) {}
              --data;
              break;
          }

      }; // Game loop

      // Can't use pos.nodes_searched() due to skipping after match
      d.movesCnt += data - gameOfsPtr - 5; // 4+1 moves for game ofs and result
  }

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
      matches += th->scout.matches.size();
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
      {
          std::cout << tab << indent4 << "{ \"ofs\": " << m.gameOfs
                                      << ", \"ply\": [";
          std::string comma;
          for (auto& p : m.plies)
          {
              std::cout << comma << p;
              comma = ", ";
          }

          std::cout << "] },";
      }

  std::cout << tab << "]\n}" << std::endl;
}


void parse_condition(Scout::Data& data, const json& item, int streakId = 0) {

  Condition cond;
  cond.streakId = streakId;

  if (item.find("result") != item.end())
  {
      GameResult result =  item["result"] == "1-0" ? WhiteWin
                         : item["result"] == "0-1" ? BlackWin
                         : item["result"] == "1/2-1/2" ? Draw
                         : item["result"] == "*" ? Unknown : Invalid;

      if (result != Invalid)
      {
          cond.result = result;
          cond.rules.push_back(RuleResult);
      }
  }

  if (item.find("sub-fen") != item.end())
  {
      for (const auto& fen : item["sub-fen"])
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

          cond.subfens.push_back(f);
      }
      cond.rules.push_back(Scout::RuleSubFen);
  }

  if (item.find("material") != item.end())
  {
      StateInfo st;
      for (const auto& mat : item["material"])
          cond.matKeys.push_back(Position().set(mat, WHITE, &st).material_key());
      cond.rules.push_back(Scout::RuleMaterial);
  }

  if (item.find("stm") != item.end())
  {
      auto rule = item["stm"] == "WHITE" ? Scout::RuleWhite : Scout::RuleBlack;
      cond.rules.push_back(rule);
  }

  if (cond.rules.size())
  {
      cond.rules.push_back(Scout::RuleMatchedCondition);
      data.conditions.push_back(cond);
  }
}

void parse_streak(Scout::Data& data, const json& streak) {

  static int streakId;

  ++streakId;
  for (const json& item : streak)
      parse_condition(data, item, streakId);
}

void parse_sequence(Scout::Data& data, const json& sequence) {

  for (const json& item : sequence)
      if (item.find("streak") != item.end())
          parse_streak(data, item["streak"]);
      else
          parse_condition(data, item);
}


/// parse_query() read a JSON input, extract the requested rules and fill the
/// Scout::Data struct to be used during the search.

void parse_query(Scout::Data& data, std::istringstream& is) {

  /* Some JSON queries:

      { "sub-fen": "8/8/p7/8/8/1B3N2/8/8" }
      { "sub-fen": "8/8/8/8/1k6/8/8/8", "material": "KBNKP" }
      { "material": "KBNKP", "stm": "WHITE" }
      { "material": "KNNK", "result": "1-0" }
      { "sub-fen": ["8/8/8/q7/8/8/8/8", "8/8/8/r7/8/8/8/8"] }
      { "sequence": [ { "sub-fen": "8/3q4/8/8/8/8/8/8" , "result": "0-1" },
                      { "sub-fen": "2q5/8/8/8/8/8/8/R6R" }] }

     See test.py for more examples.
  */

  json j = json::parse(is);

  /* Simplified grammar for our queries

     <query>     ::= <sequence> | <streak> | <condition>
     <sequence>  ::= "{ sequence: [" <condition> | streak { "," <condition> | streak } "] }"
     <streak>    ::= "{   streak: [" <condition> { "," <condition> } "] }"
     <condition> ::= "{" <rule> { "," <rule> } "}"
     <rule>      ::= string ":" <value>  // Maps into a JSON pair
     <value> = string | number | array

  */

  if (j.find("sequence") != j.end())
      parse_sequence(data, j["sequence"]);

  else if (j.find("streak") != j.end())
      parse_streak(data, j["streak"]);

  else
      parse_condition(data, j);

  // Change the end rule of the last condition
  if (data.conditions.size())
  {
      auto& lastCond = data.conditions[data.conditions.size() - 1];
      lastCond.rules[lastCond.rules.size() - 1] = Scout::RuleMatchedQuery;
  }
  else
  {
      // If query is empty push a default condition with RuleNone
      Condition cond;
      cond.rules.push_back(Scout::RuleNone);
      data.conditions.push_back(cond);
  }

}

} // namespace Scout
