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
#include <cctype>    // tolower(), isdigit()
#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "json.hpp"
#include "misc.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "uci.h"

using json = nlohmann::json;

namespace Scout {

const std::string PieceToChar(" PNBRQK  pnbrqk");


/// Helper function to verify if the move's 'from' square satisfies
/// the disambiguation rule, if any.
bool from_sq_ok(int d, Square from) {

  if (!d)
      return true;

  return d == (d > 8 ? 9 + int(rank_of(from)) : 1 + int(file_of(from)));
}


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
  size_t maxMatches = d.limit ? d.skip + d.limit : 0;
  d.matches.reserve(maxMatches ? maxMatches : 100000);
  matchPlies.reserve(128);
  Piece movedPiece = NO_PIECE;

  // Lambda helper to copy hot stuff to local variables
  auto set_condition = [&] (size_t idx) {
    cond = d.conditions.data() + idx;
    rules = cond->rules.data();
    subfens = cond->subfens.data();
    subfensEnd = subfens + cond->subfens.size();

    // Clear plies when resetting the first condition
    if (!idx)
        matchPlies.clear();

    // When starting a new streak ignore previous plies
    else if (   cond->streakId
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

  // Should point to game offset, just after the end of previous game
  assert(data == d.baseAddress || *(data-1) == MOVE_NONE);

  // Main loop, replay all games until we finish our file chunk
  while (data < end)
  {
      // First 4 moves store the game offset, skip them
      Move* gameOfsPtr = data;
      data += 4;

      // Fifth move stores the result in the 'to' square
      GameResult result = GameResult(to_sq(Move(*data)));

      // If needed, reset conditions before starting a new game
      if (condIdx != 0)
      {
          condIdx = 0;
          set_condition(condIdx);
      }

      st = states;
      Position pos = th->rootPos;
      Move move = MOVE_NONE;
      data++; // First move of the game

      // Loop across the game (that could be empty)
      do {
          assert(!move || (pos.pseudo_legal(move) && pos.legal(move)));

          Move nextMove = *data;

          // If we are looking for a streak, fail and reset as soon as last
          // matched ply is more than one half-move behind. We take care to
          // verify the last matched ply comes form the same streak.
          if (   cond->streakId
              && matchPlies.size() - streakStartPly > 0
              && matchPlies.back() != pos.nodes_searched() - 1)
          {
              assert(condIdx);

              condIdx = 0;
              set_condition(condIdx);
          }

          curRule = rules;

NextRule: // Loop across rules, early exit as soon as one fails
          switch (*curRule++) {

          case RuleNone:
              break;

          case RulePass:
              goto NextRule;

          case RuleResult:
              if (std::find(cond->results.begin(), cond->results.end(),
                            result) != cond->results.end())
                  goto NextRule;
              goto SkipToNextGame; // Shortcut: result will not change

          case RuleResultType:
              if (   !nextMove // End of game
                  && !MoveList<LEGAL>(pos).size()
                  && !!pos.checkers() == (cond->resultType == ResultMate))
                  goto NextRule;
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
                      goto NextRule;
              }
              break;

          case RuleMaterial:
              if (std::find(cond->matKeys.begin(), cond->matKeys.end(),
                            pos.material_key()) != cond->matKeys.end())
                  goto NextRule;
              break;

          case RuleImbalance:
              for (const Imbalance& imb : cond->imbalances)
                  if (   imb.nonPawnMaterial ==  pos.non_pawn_material(WHITE)
                                               - pos.non_pawn_material(BLACK)
                      && imb.pawnCount ==  pos.count<PAWN>(WHITE)
                                         - pos.count<PAWN>(BLACK))
                      goto NextRule;
              break;

          case RuleMove:
              if (cond->moveSquares & to_sq(move))
                  for (const ScoutMove& m : cond->moves)
                  {
                      if (   movedPiece != m.pc
                          || to_sq(move) != m.to
                          || m.castle != (type_of(move) == CASTLING)
                          || !from_sq_ok(m.disambiguation, from_sq(move)))
                          continue;
                      goto NextRule;
                  }
              break;

          case RuleQuietMove:
              if (move && !pos.captured_piece())
                  goto NextRule;
              break;

          case RuleCapturedPiece:
              if (move && pos.capture(move))
              {
                  PieceType pt = type_of(move) == NORMAL ? type_of(pos.captured_piece()) : PAWN;
                  if (cond->capturedFlags & (1 << int(pt)))
                      goto NextRule;
              }
              break;

          case RuleMovedPiece:
              if (move && (cond->movedFlags & (1 << int(type_of(movedPiece)))))
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
              matchPlies.clear(); // Needed for single conditon case

              // Terminate if we have collected more then enough data
              if (maxMatches && d.matches.size() >= maxMatches)
                  data = end - 2;
SkipToNextGame:
              // Skip to the end of the game after the first match
              while (*data != MOVE_NONE)
                  ++data;
              break;
          }

          // Do the move after rule checking
          move = *data;
          if (move)
              movedPiece = pos.moved_piece(move);
              pos.do_move(move, *st++, pos.gives_check(move));

      } while (*data++ != MOVE_NONE); // Exit the game loop pointing to next ofs

      // Can't use pos.nodes_searched() due to skipping moves after a match
      d.movesCnt += data - gameOfsPtr - 6; // 4+1+1 for ofs, result and MOVE_NONE
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

  size_t skip = d.skip;
  matches -= skip;

  if (d.limit)
      matches = std::min(matches, d.limit);

  std::string tab = "\n    ";
  std::string indent4 = "    ";
  std::cout << "{"
            << tab << "\"moves\": " << cnt << ","
            << tab << "\"match count\": " << matches << ","
            << tab << "\"moves/second\": " << 1000 * cnt / elapsed << ","
            << tab << "\"processing time (ms)\": " << elapsed << ","
            << tab << "\"matches\":"
            << tab << "[";

  std::string comma1;
  for (Thread* th : Threads)
  {
      for (auto& m : th->scout.matches)
      {
          if (skip)
          {
              skip--;
              continue;
          }

          if (matches <= 0)
              break;

          matches--;
          std::cout << comma1 << tab << indent4
                    << "{ \"ofs\": " << m.gameOfs
                    << ", \"ply\": [";

          std::string comma2;
          for (auto& p : m.plies)
          {
              std::cout << comma2 << p;
              comma2 = ", ";
          }

          std::cout << "] }";
          comma1 = ", ";
      }

      if (matches <= 0)
          break;
  }

  std::cout << tab << "]\n}" << std::endl;
}


ScoutMove parse_move(const std::string& san, Color c) {

  ScoutMove m = ScoutMove();

  if (san.empty())
      return m;

  if (san[0] == 'O')
  {
      m.pc = make_piece(c, KING);
      m.to = relative_square(c, san == "O-O" ? SQ_H1 : SQ_A1); // King captures rook
      m.castle = true;
      return m;
  }

  if (san.find("=") != std::string::npos)
  {
      size_t idx = san.find("=");
      Piece promotion = Piece(PieceToChar.find(san[idx + 1]));
      m.pc = make_piece(c, PAWN);
      m.to = make_square(File(san[idx - 2] - 'a'), Rank(san[idx - 1] - '1'));
      m.promotion = type_of(promotion);
      return m;
  }

  if (san[0] >= 'a' && san[0] <= 'h')
      m.pc = make_piece(c, PAWN);

  else if (PieceToChar.find(san[0]) != std::string::npos)
  {
      Piece pp = Piece(PieceToChar.find(san[0]));
      m.pc = make_piece(c, type_of(pp));
  }
  else
      return m;

  size_t toIdx = san.size() - 2;
  if (san.back() == '+' || san.back() == '#')
      toIdx--;

  m.to = make_square(File(san[toIdx] - 'a'), Rank(san[toIdx + 1] - '1'));

  if (type_of(m.pc) != PAWN && toIdx > 1) // Disambiguation
  {
      char d = san[toIdx - 1];
      m.disambiguation = ::isdigit(d) ? 9 + (d - '1') : 1 + (d - 'a');
  }

  return m;
}


void parse_condition(Scout::Data& data, const json& item, int streakId = 0) {

  Condition cond = Condition();
  cond.streakId = streakId;

  if (item.count("result"))
  {
      for (const auto& res : item["result"])
      {
          GameResult result =  res == "1-0" ? WhiteWin
                             : res == "0-1" ? BlackWin
                             : res == "1/2-1/2" ? Draw
                             : res == "*" ? Unknown : Invalid;

          if (result != Invalid)
              cond.results.push_back(result);
      }
      if (cond.results.size())
          cond.rules.push_back(RuleResult);
  }

  if (item.count("result-type"))
  {
      ResultType result =  item["result-type"] == "mate"      ? ResultMate
                         : item["result-type"] == "stalemate" ? ResultStalemate
                                                              : ResultNone;
      if (result != ResultNone)
      {
          cond.resultType = result;
          cond.rules.push_back(RuleResultType);
      }
  }

  if (item.count("sub-fen"))
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
      if (cond.subfens.size())
          cond.rules.push_back(RuleSubFen);
  }

  if (item.count("material"))
  {
      StateInfo st;
      for (const auto& mat : item["material"])
          cond.matKeys.push_back(Position().set(mat, WHITE, &st).material_key());
      if (cond.matKeys.size())
          cond.rules.push_back(RuleMaterial);
  }

  if (item.count("imbalance"))
  {
      for (std::string code : item["imbalance"])
      {
          StateInfo st;
          Imbalance imb;
          auto v = code.find('v');
          if (v == std::string::npos)
              continue;
          code.replace(v, 1, "K").insert(0, "K");
          const Position& pos = Position().set(code, WHITE, &st);
          imb.nonPawnMaterial = pos.non_pawn_material(WHITE) - pos.non_pawn_material(BLACK);
          imb.pawnCount = pos.count<PAWN>(WHITE) - pos.count<PAWN>(BLACK);
          cond.imbalances.push_back(imb);
      }
      if (cond.imbalances.size())
          cond.rules.push_back(RuleImbalance);
  }

  if (item.count("white-move"))
      for (const auto& move : item["white-move"])
      {
          const ScoutMove& m = parse_move(move, WHITE);
          if (m.pc != NO_PIECE)
          {
              cond.moves.push_back(m);
              cond.moveSquares |= m.to;
          }
      }

  if (item.count("black-move"))
      for (const auto& move : item["black-move"])
      {
          const ScoutMove& m = parse_move(move, BLACK);
          if (m.pc != NO_PIECE)
          {
              cond.moves.push_back(m);
              cond.moveSquares |= m.to;
          }
      }

  if (cond.moves.size())
      cond.rules.push_back(RuleMove);

  if (item.count("captured"))
  {
      const std::string str = item["captured"];
      for (char ch : str)
          if (PieceToChar.find(ch) != std::string::npos)
          {
              Piece pc = Piece(PieceToChar.find(ch));
              cond.capturedFlags |= 1 << int(type_of(pc));
          }
      cond.rules.push_back(cond.capturedFlags ? RuleCapturedPiece : RuleQuietMove);
  }

  if (item.count("moved"))
  {
      const std::string str = item["moved"];
      for (char ch : str)
          if (PieceToChar.find(ch) != std::string::npos)
          {
              Piece pc = Piece(PieceToChar.find(ch));
              cond.movedFlags |= 1 << int(type_of(pc));
          }
      if (cond.movedFlags)
          cond.rules.push_back(RuleMovedPiece);
  }

  if (item.count("stm"))
  {
      std::string stm = item["stm"];
      std::transform(stm.begin(), stm.end(), stm.begin(), ::tolower);

      if (stm == "white" || stm == "black")
          cond.rules.push_back(stm == "white" ? RuleWhite : RuleBlack);
  }

  if (item.count("pass"))
      cond.rules.push_back(RulePass);

  if (cond.rules.size())
  {
      cond.rules.push_back(RuleMatchedCondition);
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
      if (item.count("streak"))
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

     Simplified grammar for our queries

     <query>     ::= <sequence> | <streak> | <condition>
     <sequence>  ::= "{ "sequence": [" <condition> | streak { "," <condition> | streak } "] }"
     <streak>    ::= "{   "streak": [" <condition> { "," <condition> } "] }"
     <condition> ::= "{" <rule> { "," <rule> } "}"
     <rule>      ::= string ":" <value>  // Maps into a JSON pair
     <value> = string | number | array

  */

  json j = json::parse(is);

  if (j.count("skip"))
      data.skip = j["skip"];

  if (j.count("limit"))
      data.limit = j["limit"];

  if (j.count("sequence"))
      parse_sequence(data, j["sequence"]);

  else if (j.count("streak"))
      parse_streak(data, j["streak"]);

  else
      parse_condition(data, j);

  // Change the end rule of the last condition
  if (data.conditions.size())
      data.conditions.back().rules.back() = RuleMatchedQuery;
  else
  {
      // If query is empty push a default condition with RuleNone
      Condition cond;
      cond.rules.push_back(RuleNone);
      data.conditions.push_back(cond);
  }

}

} // namespace Scout
