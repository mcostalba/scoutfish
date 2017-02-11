## Overview

Run powerful and flexible queries on **very big chessdatabases** and with **very high speed**.

Start building an index out of a [PGN](https://en.wikipedia.org/wiki/Portable_Game_Notation) file:

    ./scoutfish make my_big_db.pgn

Scoutfish will create a file called _my_big_db.scout_ with the needed bits to make
the queries lightning fast. Queries are written in [JSON](https://en.wikipedia.org/wiki/JSON)
format that is human-readable, well supported in most languages and very simple.
Search result will be in JSON too.

You can run Scoutfish from the command line:

    ./scoutfish scout my_big_db.scout { "sub-fen": "8/8/p7/8/8/1B3N2/8/8" }

To find all the games that match the given **sub-fen** condition, i.e. all the
games with at least one position with a black pawn in _a6_, a white bishop in
_b3_ and a white knight in _f3_. Output will be like:

~~~~
{
    "moves": 14922,
    "match count": 8,
    "moves/second": 3730500,
    "processing time (ms)": 4,
    "matches":
    [
        { "ofs": 75129, "ply": [11] },
        { "ofs": 80890, "ply": [11] },
        { "ofs": 342346, "ply": [13] },
        { "ofs": 346059, "ply": [13] },
        { "ofs": 375551, "ply": [21] },
        { "ofs": 484182, "ply": [29] },
        { "ofs": 486999, "ply": [29] },
        { "ofs": 536474, "ply": [13] }
    ]
}
~~~~

After some header, there is a list of matches, each match reports an offset
(in bytes) in the original _my_big_db.pgn_ file, pointing at the beginning of
the matching game and the ply number: this is the number of (half) moves before
reaching the first position in the game that satisfies the given condition.

In case you call Scoutfish from a higher level tool, like a GUI or a web interface,
it is better to run in interactive mode:

~~~~
./scoutfish
setoption name threads value 8
scout my_big_db.scout { "sub-fen": "8/8/8/8/1k6/8/8/8", "material": "KBNKP" }
scout my_big_db.scout { "white-move": "O-O-O" }
quit
~~~~

Scoutfish is strictly derived from [Stockfish](https://stockfishchess.org/) so,
yes, it understands [UCI commands](http://wbec-ridderkerk.nl/html/UCIProtocol.html),
like _setoption_, that we use to increase thread number according to our hardware:
the search speed will increase accordingly!

Above examples show how to look for a specific **material distribution** and **move**
and how to compose a **multi-rule condition**: a position should satisfy all the rules
to match the condition.

You are not limited to search for a single _sub-fen_, the following condition:

    { "sub-fen": ["8/8/8/q7/8/8/8/8", "8/8/8/r7/8/8/8/8"] }

Will find all the positions with a black queen **or** a black rook in a5. There
is no limit to the size of the _sub-fen_ list, enabling to compose very powerful
requests.

The position **full FEN** is just a special _sub-fen_, so:

    { "sub-fen": ["rnbqkbnr/pp1p1ppp/2p5/4p3/3PP3/8/PPP2PPP/RNBQKBNR",
                  "rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R"] }

Will search for all the games with a _Caro-Kann_ or a _Sicilian_ opening.


## Rules

Rules allow to look for very specific occurrences in the game. We have already
seen some of them, like _sub-fen_, but there are many more.


##### sub-fen

Find all games with a position matching the sub-fen pattern given in
PGN notation. Support lists.


##### result

Find all games with a given result in PGN notation, like "1-0" or "1/2-1/2"

    { "sub-fen": "8/pp1p1ppp/2p5/4p3/3PP3/8/PPP2PPP/8", "result": "1-0" }


##### material

Find all games with a given material distribution, i.e. the given pieces,
no matter where placed. Support lists.

    { "material": ["KBNKNN", "KBNPKNN"] }

To find all end-games with white Knight and Bishop plus one optional pawn
against two black Knights.


##### imbalance

Find all games with a given material imbalance. Support lists.

    { "imbalance": ["PPPv", "PPv"] }
    { "imbalance": "PPvN" }


To find all games with 3 or 2 pawns advantage for white and all games
where white is above 2 pawns but down of a knight.


##### white-move / black-move

Find all games with a given move in PGN notation. Support lists.

    {"white-move": "e8=Q"}
    {"black-move": ["O-O-O", "O-O"]}
    {"black-move": "Rac1"}

To find all games with white's queen promotion in _e8_ and all games
with a black castling, no matter if long or short. Rule supports
SAN notation with a disambiguation, like _Rac1_.


##### moved / captured

Find all games with a given moved and/or captured piece. Pieces are listed
in a single string.

    {"moved": "KP", "captured": "Q" }
    {"captured": "" }

To find all games with a queen captured by a king or a pawn and all games
with a quiet move (eventually to be used in a multi-rule condition).


##### stm

This rule matches the given side to move, that can be "white" or "black"
Usually it is used in a multi-rule condition.

    {"stm": "black", "captured": "QR" }

To find all games where black side captures a queen or a rook.


## Sequences

A _sequence_ is a powerful feature of Scoutfish to look for games that satisfy
more than one condition at *different times in game*. This is very useful in
looking for a piece path from a position. Typical tournament player questions
are: how does one maneuver the bishop in this opening, should we go f1-d3-c2 or
f1-b5-a4. When should we select which maneuver?

    { "sequence": [ { "sub-fen": "r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/2N2N2/PPPP1PPP/R1BQK2R", "result": "1-0" },
                    { "sub-fen": "8/8/8/8/2B5/8/8/8" },
                    { "sub-fen": "8/8/8/8/8/5B2/8/8" } ] }

The above _sequence_ will find all the games won by white, with _Four Knights
Spanish Variation_ opening and with bishop maneuvering to b5-c4-f3.

A _sequence_ is mainly a list of conditions: a game should match first condition,
then the second one (eventually later in the game) and so on for all the list,
that can be arbitrary long. Output of the search will be like:

~~~~
{
    "moves": 28796,
    "match count": 2,
    "moves/second": 5759200,
    "processing time (ms)": 5,
    "matches":
    [
        { "ofs": 19054, "ply": [7, 15, 17] },
        { "ofs": 20653, "ply": [7, 15, 17] }
    ]
}
~~~~

Where _ply_ list will show the matching ply for each condition in the sequence.


## Streaks

A _streak_ is a special kind of sequence. It is defined like a sequence and has
all the sequence properties, but it is different in two key points:

- Conditions in a streak should be satisfied in consecutive (half) moves
- A streak can appear nested in a bigger, outer sequence

Mainly a streak is like a sequence but with the added constrain that the conditions
should be satisfied one-by-one along consecutive moves. You may want to use a
streak to look for a white's pawn-down imbalance that should persist for at least
few moves to be sure we are not in the middle of a capture-recapture combination:

~~~~
{ "streak": [ { "imbalance": "vP" }, { "imbalance": "vP" }, { "imbalance": "vP" } ] }
~~~~

From chess perspective, say you want to find games with a clearance sacrifice in
the Benoni for white. Plan of e5, dxe5, followed by f5 and then Ne4 for white.
The first three moves are in a streak, but the last move might be delayed by a
move (but is also played immediately):

~~~~
{ "sequence": [ { "sub-fen": "rnbqkb1r/pp1p1ppp/4pn2/2pP4/2P5/2N5/PP2PPPP/R1BQKBNR"},
                { "streak": [ { "white-move": "e5"}, { "black-move": "dxe5"}, { "white-move": "f5"} ] },
                { "white-move": "Ne4"} ] }
~~~~

The above sequence, first checks for Benoni opening, then checks for the
**consecutives** e5, dxe5, f5 then finally by the (possibly delayed) Ne4.

Some rules like _captured_ are very suitable to be used in a streak:

~~~~
{ "streak": [ { "captured": "" }, { "stm": "white", "captured": "Q" }, { "captured": "" } ] }
~~~~

To find all games where white captures a net queen, i.e. not in a capture-recapture
combination.


## Python wrapper

As a typical UCI chess engine, also Scoutfish is not intended to be exposed to the
user directly, eventually a GUI or a web interface will handle the user interaction,
composing the query and later presenting the results in a graphical form, ensuring
a user friendly experience.

To easy integration with higher level tools, a Python wrapper is provided through
**scoutfish.py** file:

~~~~python
from scoutfish import Scoutfish

p = Scoutfish()
p.setoption('threads', 4)  # Will use 4 threads for searching
p.open('my_big.pgn')

q = {'white-move': 'O-O-O'}  # Our query, defined as a simple dict
result = p.scout(q)

num = result['match count']
print('Found ' + str(num) + ' games')

# Run again the query but limiting output to max 10 games and skipping
# the first 50 results. This is useful for pagination.
q = {'skip': 50, 'limit': 10, 'white-move': 'O-O-O'}

result = p.scout(q)
games = p.get_games(result['matches'])  # Load the pgn games from my_big.pgn

for g in games:
    print(g['pgn'])

p.close()
~~~~
