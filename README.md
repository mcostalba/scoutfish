# Scoutfish

Search a chess DB by means of powerful and flexible queries. Scoutfish is
designed to run on **very big chess databases** and with **very high speed**.

Start building an index out of a [PGN](https://en.wikipedia.org/wiki/Portable_Game_Notation) file:

    ./scoutfish make my_big_db.pgn

Scoutfish will create a file called _my_big_db.bin_ with the needed info to make the queries lightning fast.
Queries are written in [JSON format](https://en.wikipedia.org/wiki/JSON) that is human-readable, well supported
in most languages and very simple. Also the query result will be in JSON.

You can run scoutfish from the command line:

    ./scoutfish scout my_big_db.bin { "sub-fen": "8/8/p7/8/8/1B3N2/8/8" }

To find all the games that match the given **sub-fen**, i.e. all the games with at least one position with
a black pawn in a6, a white bishop in b3 and a white knight in f3. Output will be like:

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

Aftrer the header, we get a list of matches, each match reports an offset (in bytes) in the original
_my_big_db.pgn_ file, pointing at the beginning of the matching game and the ply number (half move) of the first
match inside the game.

In case you call scoutfish from a higher level tool, like a GUI or a web interface, it is better to operate
it in interactive mode:

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

Above examples show how to query for a specific material distribution, for a specific
game result and how to compose the queries to create complex and very general multiple-conditions.










