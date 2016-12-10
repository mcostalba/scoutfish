## Overview

Search a chess DB by powerful and flexible queries. Scoutfish is designed to run on
**very big chess databases** and with **very high speed**.

Start building an index out of a [PGN](https://en.wikipedia.org/wiki/Portable_Game_Notation) file:

    ./scoutfish make my_big_db.pgn

Scoutfish will create a file called _my_big_db.bin_ with the needed info to make the queries
lightning fast. Queries are written in [JSON](https://en.wikipedia.org/wiki/JSON) format that is
human-readable, well supported in most languages and very simple. Query output will be in JSON too.

You can run Scoutfish from the command line:

    ./scoutfish scout my_big_db.bin { "sub-fen": "8/8/p7/8/8/1B3N2/8/8" }

To find all the games that match the given **sub-fen**, i.e. all the games with at least one position
with a black pawn in _a6_, a white bishop in _b3_ and a white knight in _f3_. Output will be like:

~~~~
{
    "moves": 14922,
    "match count": 8,
    "moves/second": 3730500,
    "processing time (ms)": 4,
    "matches":
    [
        { "ofs": 75129, "ply": 11},
        { "ofs": 80890, "ply": 11},
        { "ofs": 342346, "ply": 13},
        { "ofs": 346059, "ply": 13},
        { "ofs": 375551, "ply": 21},
        { "ofs": 484182, "ply": 29},
        { "ofs": 486999, "ply": 29},
        { "ofs": 536474, "ply": 13},
    ]
}
~~~~

After some header, there is a list of matches, each match reports an offset (in bytes) in the original
_my_big_db.pgn_ file, pointing at the beginning of the matching game and the ply number (half move) of
the first match inside the game.

In case you call Scoutfish from a higher level tool, like a GUI or a web interface, it is better to
run in interactive mode:

~~~~
./scoutfish
setoption name threads value 8
scout my_big_db.bin { "sub-fen": "8/8/8/8/1k6/8/8/8", "material": "KBNKP" }
scout my_big_db.bin { "material": "KBNKP", "stm": "WHITE" }
scout my_big_db.bin { "material": "KNNK", "result": "1-0" }
quit
~~~~

Scoutfish is strictly derived from [Stockfish](https://stockfishchess.org/) so, yes, it understands
[UCI commands](http://wbec-ridderkerk.nl/html/UCIProtocol.html), like _setoption_, that we use to
increase thread number according to our hardware: the search speed will increase accordingly!

Above examples show how to query for a specific **material distribution**, for a specific
**side to move** or **game result** and how to compose a query to match multiple conditions.

You are not limited to search for a single sub-fen, the following query:

    { "sub-fen": ["8/8/8/q7/8/8/8/8", "8/8/8/r7/8/8/8/8"] }

Will find all the positions with a black queen **or** a black rook in a5. There is no limit
to the size of the sub-fen list, enabling to compose very powerful queries.

The position **full FEN** is just a special sub-fen, so:

    { "sub-fen": ["rnbqkbnr/pp1p1ppp/2p5/4p3/3PP3/8/PPP2PPP/RNBQKBNR",
                  "rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R"] }

Will search for all the games with a _Caro-Kann_ or a _Sicilian_ opening.


## Sequences

A _sequence_ is a powerful feature of Scoutfish to look for games that satisfy more than one
condition at *different times in game*. This is very useful in looking for a piece path
from a position. Typical tournament player questions are: How does one maneuver the bishop in
this opening, should we go f1-d3-c2 or f1-b5-a4. When should we select which maneuver?

    { "sequence": [ { "sub-fen": "r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/2N2N2/PPPP1PPP/R1BQK2R", "result": "1-0" },
                    { "sub-fen": "8/8/8/8/2B5/8/8/8" },
                    { "sub-fen": "8/8/8/8/8/5B2/8/8" } ] }

The above query will find all the games won by white, with _Four Knights Spanish Variation_
opening and with bishop maneuvering to b5-c4-f3.
