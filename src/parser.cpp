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
#include "thread.h"
#include "uci.h"

namespace {

struct PGNStats {
    int64_t games;
    int64_t moves;
    int64_t fixed;
};

enum Token {
    T_NONE, T_SPACES, T_RESULT, T_MINUS, T_DOT, T_QUOTES, T_DOLLAR,
    T_LEFT_BRACKET, T_RIGHT_BRACKET, T_LEFT_BRACE, T_RIGHT_BRACE,
    T_LEFT_PARENTHESIS, T_RIGHT_PARENTHESIS, T_ZERO, T_DIGIT, T_MOVE_HEAD, TOKEN_NB
};

enum State {
    HEADER, TAG, FEN_TAG, BRACE_COMMENT, VARIATION, NUMERIC_ANNOTATION_GLYPH,
    NEXT_MOVE, MOVE_NUMBER, NEXT_SAN, READ_SAN, RESULT, STATE_NB
};

enum Step : uint8_t {
    FAIL, CONTINUE, OPEN_TAG, OPEN_BRACE_COMMENT, READ_FEN, CLOSE_FEN_TAG,
    OPEN_VARIATION, START_NAG, POP_STATE, START_MOVE_NUMBER, START_NEXT_SAN,
    CASTLE_OR_RESULT, START_READ_SAN, READ_MOVE_CHAR, END_MOVE, START_RESULT,
    END_GAME, TAG_IN_BRACE, MISSING_RESULT
};

enum MetaType {
    MOVE_TOTAL, MOVE_WIN, MOVE_DRAW
};

Token ToToken[256];
Step ToStep[STATE_NB][TOKEN_NB];
Position RootPos;

void error(Step* state, const char* data) {

    std::vector<std::string> stateDesc = {
        "HEADER", "TAG", "FEN_TAG", "BRACE_COMMENT", "VARIATION",
        "NUMERIC_ANNOTATION_GLYPH", "NEXT_MOVE", "MOVE_NUMBER",
        "NEXT_SAN", "READ_SAN", "RESULT"
    };

    for (int i = 0; i < STATE_NB; i++)
        if (ToStep[i] == state)
        {
            std::string what = std::string(data, 50);
            std::cerr << "Wrong " << stateDesc[i] << ": '"
                      << what << "' " << std::endl;
        }
    exit(0);
}

/// Convert a number of type T into a sequence of bytes in big-endian format

template<typename T> uint8_t* write(const T& n, uint8_t* data) {

    for (int i =  8 * (sizeof(T) - 1); i >= 0; i -= 8, ++data)
        *data = uint8_t(n >> i);

    return data;
}

template<bool DryRun = false>
const char* parse_game(const char* moves, const char* end, std::ofstream& ofs,
                       const char* fen, const char* fenEnd, size_t& fixed) {

    StateInfo states[1024], *st = states;
    Move gameMoves[1024], *curMove = gameMoves;
    Position pos = RootPos;
    const char *cur = moves;

    if (fenEnd != fen)
        pos.set(fen, false, st++, pos.this_thread());

    while (cur < end)
    {
        *curMove = pos.san_to_move(cur, end, fixed);
        if (*curMove == MOVE_NONE)
        {
            if (!DryRun)
            {
                const char* sep = pos.side_to_move() == WHITE ? "" : "..";
                std::cerr << "\nWrong move notation: " << sep << cur
                          << "\n" << pos << std::endl;

            }
            return cur;
        }
        else if (*curMove == MOVE_NULL)
            pos.do_null_move(*st++);
        else
            pos.do_move(*curMove, *st++, pos.gives_check(*curMove));

        while (*cur++) {} // Go to next move
        ++curMove;
    }

    if (!DryRun && fenEnd == fen)
    {
        *curMove++ = MOVE_NONE; // Game separator
        ofs.write((const char*)gameMoves, (curMove - gameMoves) * sizeof(Move));
    }

    return end;
}

void parse_pgn(void* baseAddress, uint64_t size, PGNStats& stats, std::ofstream& ofs) {

    Step* stateStack[16];
    Step**stateSp = stateStack;
    char fen[256], *fenEnd = fen;
    char moves[1024 * 8], *curMove = moves;
    char* end = curMove;
    size_t moveCnt = 0, gameCnt = 0, fixed = 0;
    char* data = (char*)baseAddress;
    char* eof = data + size;
    int stm = WHITE;
    Step* state = ToStep[HEADER];

    for (  ; data < eof; ++data)
    {
        Token tk = ToToken[*(uint8_t*)data];

        switch (state[tk])
        {
        case FAIL:
            error(state, data);
            break;

        case CONTINUE:
            break;

        case OPEN_TAG:
            *stateSp++ = state;
            state =   *(  data + 1) == 'F'
                   && *(++data + 1) == 'E'
                   && *(++data + 1) == 'N'
                   && *(++data + 1) == ' '
                   && *(++data + 1) == '"'
                   &&   ++data ? ToStep[FEN_TAG] : ToStep[TAG];
            break;

        case OPEN_BRACE_COMMENT:
            *stateSp++ = state;
            state = ToStep[BRACE_COMMENT];
            break;

        case READ_FEN:
            *fenEnd++ = *data;
            break;

        case CLOSE_FEN_TAG:
            *fenEnd++ = 0; // Zero-terminating string
            state = ToStep[TAG];
            if (strstr(fen, " b "))
                stm = BLACK;
            break;

        case OPEN_VARIATION:
            *stateSp++ = state;
            state = ToStep[VARIATION];
            break;

        case START_NAG:
            *stateSp++ = state;
            state = ToStep[NUMERIC_ANNOTATION_GLYPH];
            break;

        case POP_STATE:
            state = *(--stateSp);
            break;

        case START_MOVE_NUMBER:
            state = ToStep[MOVE_NUMBER];
            break;

        case START_NEXT_SAN:
            state = ToStep[NEXT_SAN];
            break;

        case CASTLE_OR_RESULT:
            if (data[2] != '0')
            {
                state = ToStep[RESULT];
                continue;
            }
            /* Fall through */

        case START_READ_SAN:
            *end++ = *data;
            state = ToStep[READ_SAN];
            break;

        case READ_MOVE_CHAR:
            *end++ = *data;
            break;

        case END_MOVE:
            *end++ = 0; // Zero-terminating string
            curMove = end;
            moveCnt++;
            state = ToStep[stm == WHITE ? NEXT_SAN : NEXT_MOVE];
            stm ^= 1;
            break;

        case START_RESULT:
            state = ToStep[RESULT];
            break;

        case END_GAME:
            if (*data != '\n') // Handle spaces in result, like 1/2 - 1/2
            {
                state = ToStep[RESULT];
                break;
            }
            parse_game(moves, end, ofs, fen, fenEnd, fixed);
            gameCnt++;
            end = curMove = moves;
            fenEnd = fen;
            state = ToStep[HEADER];
            stm = WHITE;
            break;

        case TAG_IN_BRACE:
             // Special case of missed brace close. Detect beginning of next game
             if (strncmp(data, "[Event ", 7))
                 break;

             /* Fall through */

        case MISSING_RESULT: // Missing result, next game already started
            parse_game(moves, end, ofs, fen, fenEnd, fixed);
            gameCnt++;
            end = curMove = moves;
            fenEnd = fen;
            state = ToStep[HEADER];
            stm = WHITE;

            *stateSp++ = state; // Fast forward into a TAG
            state = ToStep[TAG];
            break;

        default:
            assert(false);
            break;
        }
    }

    // Force accounting of last game if still pending. Many reason for this to
    // trigger: no newline at EOF, missing result, missing closing brace, etc.
    if (state != ToStep[HEADER] && end - moves)
    {
        parse_game(moves, end, ofs, fen, fenEnd, fixed);
        gameCnt++;
    }

    stats.games = gameCnt;
    stats.moves = moveCnt;
    stats.fixed = fixed;
}

} // namespace

const char* play_game(const Position& pos, Move move, const char* cur, const char* end) {

    size_t fixed;
    std::ofstream ofs;
    StateInfo st;
    Position p = pos;
    p.do_move(move, st, pos.gives_check(move));
    while (*cur++) {} // Move to next move in game
    return cur < end ? parse_game<true>(cur, end, ofs, p.fen().c_str(),
                                        nullptr, fixed) : cur;
}

namespace Parser {


void init() {

    static StateInfo st;
    const char* startFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    RootPos.set(startFEN, false, &st, Threads.main());

    ToToken['\n'] = ToToken['\r'] = ToToken[' '] = ToToken['\t'] = T_SPACES;
    ToToken['/'] = ToToken['*'] = T_RESULT;
    ToToken['-'] = T_MINUS;
    ToToken['.'] = T_DOT;
    ToToken['"'] = T_QUOTES;
    ToToken['$'] = T_DOLLAR;
    ToToken['['] = T_LEFT_BRACKET;
    ToToken[']'] = T_RIGHT_BRACKET;
    ToToken['{'] = T_LEFT_BRACE;
    ToToken['}'] = T_RIGHT_BRACE;
    ToToken['('] = T_LEFT_PARENTHESIS;
    ToToken[')'] = T_RIGHT_PARENTHESIS;
    ToToken['0'] = T_ZERO;
    ToToken['1'] = ToToken['2'] = ToToken['3'] =
    ToToken['4'] = ToToken['5'] = ToToken['6'] = ToToken['7'] =
    ToToken['8'] = ToToken['9'] = T_DIGIT;
    ToToken['a'] = ToToken['b'] = ToToken['c'] = ToToken['d'] =
    ToToken['e'] = ToToken['f'] = ToToken['g'] = ToToken['h'] =
    ToToken['N'] = ToToken['B'] = ToToken['R'] = ToToken['Q'] =
    ToToken['K'] = ToToken['O'] = ToToken['o'] = T_MOVE_HEAD;

    // Trailing move notations are ignored because SAN detector
    // does not need them and in some malformed PGN they appear
    // one blank apart from the corresponding move.
    ToToken['!'] = ToToken['?'] = ToToken['+'] = ToToken['#'] = T_SPACES;

    // STATE = HEADER
    //
    // Between tags, before game starts. Accept anything
    for (int i = 0; i < TOKEN_NB; i++)
        ToStep[HEADER][i] = CONTINUE;

    ToStep[HEADER][T_LEFT_BRACKET] = OPEN_TAG;
    ToStep[HEADER][T_LEFT_BRACE  ] = OPEN_BRACE_COMMENT;
    ToStep[HEADER][T_DIGIT       ] = START_MOVE_NUMBER;
    ToStep[HEADER][T_ZERO        ] = START_RESULT;
    ToStep[HEADER][T_RESULT      ] = START_RESULT;

    // STATE = TAG
    //
    // Between brackets in header section, generic tag
    for (int i = 0; i < TOKEN_NB; i++)
        ToStep[TAG][i] = CONTINUE;

    ToStep[TAG][T_RIGHT_BRACKET] = POP_STATE;

    // STATE = FEN_TAG
    //
    // Special tag to set a position from a FEN string
    for (int i = 0; i < TOKEN_NB; i++)
        ToStep[FEN_TAG][i] = READ_FEN;

    ToStep[FEN_TAG][T_QUOTES] = CLOSE_FEN_TAG;

    // STATE = BRACE_COMMENT
    //
    // Comment in braces, can appear almost everywhere. Note that brace comments
    // do not nest according to PGN standard.
    for (int i = 0; i < TOKEN_NB; i++)
        ToStep[BRACE_COMMENT][i] = CONTINUE;

    ToStep[BRACE_COMMENT][T_RIGHT_BRACE ] = POP_STATE;
    ToStep[BRACE_COMMENT][T_LEFT_BRACKET] = TAG_IN_BRACE; // Missed closing brace

    // STATE = VARIATION
    //
    // For the moment variations are ignored
    for (int i = 0; i < TOKEN_NB; i++)
        ToStep[VARIATION][i] = CONTINUE;

    ToStep[VARIATION][T_RIGHT_PARENTHESIS] = POP_STATE;
    ToStep[VARIATION][T_LEFT_PARENTHESIS ] = OPEN_VARIATION; // Nested
    ToStep[VARIATION][T_LEFT_BRACE       ] = OPEN_BRACE_COMMENT;

    // STATE = NUMERIC_ANNOTATION_GLYPH
    //
    // Just read a single number
    for (int i = 0; i < TOKEN_NB; i++)
        ToStep[NUMERIC_ANNOTATION_GLYPH][i] = POP_STATE;

    ToStep[NUMERIC_ANNOTATION_GLYPH][T_ZERO ] = CONTINUE;
    ToStep[NUMERIC_ANNOTATION_GLYPH][T_DIGIT] = CONTINUE;

    // STATE = NEXT_MOVE
    //
    // Check for the beginning of the next move number
    for (int i = 0; i < TOKEN_NB; i++)
        ToStep[NEXT_MOVE][i] = CONTINUE;

    ToStep[NEXT_MOVE][T_LEFT_PARENTHESIS] = OPEN_VARIATION;
    ToStep[NEXT_MOVE][T_LEFT_BRACE      ] = OPEN_BRACE_COMMENT;
    ToStep[NEXT_MOVE][T_LEFT_BRACKET    ] = MISSING_RESULT;
    ToStep[NEXT_MOVE][T_DOLLAR          ] = START_NAG;
    ToStep[NEXT_MOVE][T_RESULT          ] = START_RESULT;
    ToStep[NEXT_MOVE][T_ZERO            ] = START_RESULT;
    ToStep[NEXT_MOVE][T_DOT             ] = FAIL;
    ToStep[NEXT_MOVE][T_MOVE_HEAD       ] = FAIL;
    ToStep[NEXT_MOVE][T_MINUS           ] = FAIL;
    ToStep[NEXT_MOVE][T_DIGIT           ] = START_MOVE_NUMBER;

    // STATE = MOVE_NUMBER
    //
    // Continue until a dot is found, to tolerate missing dots,
    // stop at first space, then start NEXT_SAN that will handle
    // head trailing spaces. We can alias with a result like 1-0 or 1/2-1/2
    ToStep[MOVE_NUMBER][T_ZERO  ] = CONTINUE;
    ToStep[MOVE_NUMBER][T_DIGIT ] = CONTINUE;
    ToStep[MOVE_NUMBER][T_RESULT] = START_RESULT;
    ToStep[MOVE_NUMBER][T_MINUS ] = START_RESULT;
    ToStep[MOVE_NUMBER][T_SPACES] = START_NEXT_SAN;
    ToStep[MOVE_NUMBER][T_DOT   ] = START_NEXT_SAN;

    // STATE = NEXT_SAN
    //
    // Check for the beginning of the next move SAN
    for (int i = 0; i < TOKEN_NB; i++)
        ToStep[NEXT_SAN][i] = CONTINUE;

    ToStep[NEXT_SAN][T_LEFT_PARENTHESIS] = OPEN_VARIATION;
    ToStep[NEXT_SAN][T_LEFT_BRACE      ] = OPEN_BRACE_COMMENT;
    ToStep[NEXT_SAN][T_LEFT_BRACKET    ] = MISSING_RESULT;
    ToStep[NEXT_SAN][T_DOLLAR          ] = START_NAG;
    ToStep[NEXT_SAN][T_RESULT          ] = START_RESULT;
    ToStep[NEXT_SAN][T_ZERO            ] = CASTLE_OR_RESULT;  // 0-0 or 0-1
    ToStep[NEXT_SAN][T_DOT             ] = CONTINUE;          // Like 4... exd5
    ToStep[NEXT_SAN][T_DIGIT           ] = START_MOVE_NUMBER; // Same as above
    ToStep[NEXT_SAN][T_MOVE_HEAD       ] = START_READ_SAN;
    ToStep[NEXT_SAN][T_MINUS           ] = START_READ_SAN;    // Null move "--"

    // STATE = READ_SAN
    //
    // Just read a single move SAN until a space is reached
    for (int i = 0; i < TOKEN_NB; i++)
        ToStep[READ_SAN][i] = READ_MOVE_CHAR;

    ToStep[READ_SAN][T_SPACES    ] = END_MOVE;
    ToStep[READ_SAN][T_LEFT_BRACE] = OPEN_BRACE_COMMENT;

    // STATE = RESULT
    //
    // Ignore anything until a space is reached
    for (int i = 0; i < TOKEN_NB; i++)
        ToStep[RESULT][i] = CONTINUE;

    ToStep[RESULT][T_SPACES] = END_GAME;
}

void make_db(std::istringstream& is) {

    PGNStats stats;
    uint64_t mapping, size;
    void* baseAddress;
    std::string dbName;

    is >> dbName;

    if (dbName.empty())
    {
        std::cerr << "Missing PGN file name..." << std::endl;
        exit(0);
    }

    mem_map(dbName.c_str(), &baseAddress, &mapping, &size);

    size_t lastdot = dbName.find_last_of(".");
    if (lastdot != std::string::npos)
        dbName = dbName.substr(0, lastdot);
    dbName += ".bin";
    std::ofstream ofs;
    ofs.open(dbName, std::ofstream::out | std::ofstream::binary);
    Move first = MOVE_NONE;
    ofs.write((const char*)&first, sizeof(Move));

    std::cerr << "\nProcessing...";

    TimePoint elapsed = now();

    parse_pgn(baseAddress, size, stats, ofs);

    elapsed = now() - elapsed + 1; // Ensure positivity to avoid a 'divide by zero'

    mem_unmap(baseAddress, mapping);

    size_t dbSize = ofs.tellp();
    ofs.close();

    std::cerr << "done\n"
              << "\nGames: " << stats.games
              << "\nMoves: " << stats.moves
              << "\nIncorrect moves: " << stats.fixed
              << "\nGames/second: " << 1000 * stats.games / elapsed
              << "\nMoves/second: " << 1000 * stats.moves / elapsed
              << "\nMBytes/second: " << float(size) / elapsed / 1000
              << "\nDB file: " << dbName
              << "\nDB file size: " << dbSize
              << "\nProcessing time (ms): " << elapsed << "\n" << std::endl;
}

}
