#!/usr/bin/env python

import argparse
import hashlib
import json
import os
import sys
from scoutfish import Scoutfish

SCOUTFISH = './scoutfish.exe' if 'nt' in os.name else './scoutfish'

QUERIES = [
    {'q': {'sub-fen': 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR', 'stm': 'WHITE'}, 'sig': '869e89b'},
    {'q': {'sub-fen': 'rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR', 'stm': 'BLACK'}, 'sig': 'ec5311d'},
    {'q': {'sub-fen': '8/8/p7/8/8/1B3N2/8/8'}, 'sig': '7e1b65e'},
    {'q': {'sub-fen': '8/8/8/8/1k6/8/8/8', 'result': '1/2-1/2'}, 'sig': 'b56bf5e'},
    {'q': {'sub-fen': ['8/8/8/q7/8/8/8/8', '8/8/8/r7/8/8/8/8']}, 'sig': '051c204'},
    {'q': {'material': 'KQRRBNPPPPKQRRNNPPPP', 'stm': 'BLACK'}, 'sig': 'aa091ce'},
    {'q': {'material': 'KQRRBNNPPPPKQRRBNNPPPP', 'result': '0-1'}, 'sig': '7e121f7'},
    {'q': {'material': ['KRBPPPKRPPP', 'KRPPPKRPPP']}, 'sig': 'ba89092'},
    {'q': {'white-move': 'Nb7'}, 'sig': 'c24c598'},
    {'q': {'black-move': 'c3'}, 'sig': '965bb73'},
    {'q': {'black-move': 'e1=Q'}, 'sig': '9430748'},
    {'q': {'black-move': 'O-O'}, 'sig': '58ac19c'},
    {'q': {'skip': 200, 'limit': 100, 'black-move': 'O-O'}, 'sig': '634255e'},
    {'q': {'black-move': 'O-O-O'}, 'sig': '6059a8c'},
    {'q': {'black-move': ['O-O-O', 'O-O']}, 'sig': '65ddb21'},
    {'q': {'white-move': ['a7', 'b7']}, 'sig': 'a3a99e0'},
    {'q': {'imbalance': 'vPP'}, 'sig': '742217a'},
    {'q': {'imbalance': ['BvN', 'NNvB']}, 'sig': '408c38a'},
    {'q': {'moved': 'KP', 'captured': 'Q'}, 'sig': '84d49e7'},

    {'q': {'sub-fen': ['rnbqkbnr/pp1p1ppp/2p5/4p3/3PP3/8/PPP2PPP/RNBQKBNR',
                       'rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R']},
     'sig': '5bb64c8'},

    {'q': {'sequence': [{'sub-fen': '8/3p4/8/8/8/8/8/8', 'result': '1-0'},
                        {'sub-fen': '8/2q5/8/8/8/8/8/R6R'}]},
     'sig': '4146400'},

    {'q': {'sequence': [{'sub-fen': 'r1bqkb1r/pppp1ppp/2n2n2/1B2p3/'
                                    '4P3/2N2N2/PPPP1PPP/R1BQK2R'},
                        {'sub-fen': '8/8/8/8/2B5/8/8/8'},
                        {'sub-fen': '8/8/8/8/8/5B2/8/8'}]},
     'sig': '636f55b'},

    {'q': {'streak': [{'sub-fen': 'r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/2N2N2/PPPP1PPP/R1BQK2R'},
                      {'result': '0-1'}, {'result': '0-1'}]},
     'sig': '13302dd'},

    {'q': {'sequence': [{'sub-fen': 'rnbqkb1r/pp1p1ppp/4pn2/2pP4/2P5/2N5/PP2PPPP/R1BQKBNR'},
                        {'streak': [{'white-move': 'e5'}, {'black-move': 'dxe5'}, {'white-move': 'f5'}]},
                        {'white-move': 'Ne4'}]},
     'sig': '7ada661'},

    {'q': {'sequence': [{'sub-fen': 'rnbqkb1r/pp1p1ppp/4pn2/2pP4/2P5/2N5/PP2PPPP/R1BQKBNR'},
                        {'streak': [{'white-move': 'e5'}, {'black-move': 'dxe5'}, {'white-move': 'f5'},
                                    {'white-move': 'Ne4'}]}]},
     'sig': 'cab98f3'},

    {'q': {'sequence': [{'sub-fen': 'rnbqkb1r/pp1p1ppp/4pn2/2pP4/2P5/2N5/PP2PPPP/R1BQKBNR'},
                        {'streak': [{'white-move': 'e5'}, {'pass': ''}, {'white-move': 'f5'}]},
                        {'white-move': 'Ne4'}]},
     'sig': '7ada661'},

    {'q': {'streak': [{'imbalance': 'NNvB'}, {'imbalance': 'NNvB'}, {'imbalance': 'NNvB'}]},
     'sig': '6974053'},

    {'q': {'streak': [{'moved': 'P', 'captured': 'Q'}, {'captured': ''}]},
     'sig': 'ecebf2f'},
]


def signature(matches):
    d = str(matches)
    sha = hashlib.sha1(d).hexdigest()
    return sha[:7]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run a set of test queries')
    parser.add_argument('--pgn', default='../pgn/famous_games.pgn')
    parser.add_argument('--path', default=SCOUTFISH)
    parser.add_argument('--threads', default=1)
    args = parser.parse_args()

    if not os.path.isfile(args.path):
        print("File {} does not exsist".format(args.path))
        sys.exit(0)

    if not os.path.isfile(args.pgn):
        print("File {} does not exsist".format(args.pgn))
        sys.exit(0)

    # Spawn scoutfish
    sys.stdout.write('Making index...')
    sys.stdout.flush()
    p = Scoutfish(args.path)
    p.setoption('threads', args.threads)
    p.open(args.pgn)
    p.make()  # Force rebuilding of DB index
    print('done')

    # Run test queries
    for cnt, e in enumerate(QUERIES):
        sys.stdout.write('Query ' + str(cnt + 1) + '...')
        sys.stdout.flush()
        result = p.scout(e['q'])
        num = str(result['match count'])
        matches = result['matches']
        games = p.get_games(matches[:10])
        headers = p.get_game_headers(games)
        sig1 = signature(matches)
        sig2 = signature(games)
        sig3 = signature(headers)
        sig = signature(sig1 + sig2 + sig3)
        if (e['sig'] == sig):
            print('OK (' + num + ')')
        else:
            print('FAIL (' + num + ')')
        p.before = ''

    p.close()
