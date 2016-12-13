#!/usr/bin/env python

import argparse
import hashlib
import json
import os
import sys

from scoutfish import Scoutfish

QUERIES = [
    {'q': {'sub-fen': '8/8/p7/8/8/1B3N2/8/8'}, 'sig': '6265541'},
    {'q': {'sub-fen': '8/8/8/8/1k6/8/8/8', 'result': '1/2-1/2'}, 'sig': '0369452'},
    {'q': {'sub-fen': ['8/8/8/q7/8/8/8/8', '8/8/8/r7/8/8/8/8']}, 'sig': 'd0eaae9'},
    {'q': {'material': 'KQRRBNPPPPKQRRNNPPPP', 'stm': 'BLACK'}, 'sig': 'e4caa92'},
    {'q': {'material': 'KQRRBNNPPPPKQRRBNNPPPP', 'result': '0-1'}, 'sig': 'c892fb9'},
    {'q': {'material': ['KRBPPPKRPPP', 'KRPPPKRPPP']}, 'sig': '377a2bb'},
    {'q': {'white-move': 'Nb7'}, 'sig': '6089546'},
    {'q': {'black-move': 'c3'}, 'sig': '8357cde'},
    {'q': {'black-move': 'e1=Q'}, 'sig': '03f16ba'},
    {'q': {'black-move': 'O-O'}, 'sig': 'ea3c83b'},
    {'q': {'black-move': 'O-O-O'}, 'sig': '9f3fd79'},
    {'q': {'black-move': ['O-O-O', 'O-O']}, 'sig': '40d4f99'},
    {'q': {'white-move': ['a7', 'b7']}, 'sig': '295355b'},

    {'q': {'sub-fen': ['rnbqkbnr/pp1p1ppp/2p5/4p3/3PP3/8/PPP2PPP/RNBQKBNR',
                       'rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R']},
     'sig': '4dace7c'},

    {'q': {'sequence': [{'sub-fen': '8/3p4/8/8/8/8/8/8', 'result': '1-0'},
                        {'sub-fen': '8/2q5/8/8/8/8/8/R6R'}]},
     'sig': 'bda360d'},

    {'q': {'sequence': [{'sub-fen': 'r1bqkb1r/pppp1ppp/2n2n2/1B2p3/'
                                    '4P3/2N2N2/PPPP1PPP/R1BQK2R'},
                        {'sub-fen': '8/8/8/8/2B5/8/8/8'},
                        {'sub-fen': '8/8/8/8/8/5B2/8/8'}]},
     'sig': '7c244bb'},

    {'q': {'streak': [{'sub-fen': 'r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/2N2N2/PPPP1PPP/R1BQK2R'},
                      {'result': '0-1'}, {'result': '0-1'}]},
     'sig': '68b4158'},

    {'q': {'sequence': [{'sub-fen': 'rnbqkb1r/pp1p1ppp/4pn2/2pP4/2P5/2N5/PP2PPPP/R1BQKBNR'},
                        {'streak': [{'white-move': 'e5'}, {'black-move': 'dxe5'}, {'white-move': 'f5'}]},
                        {'white-move': 'Ne4'}]},
     'sig': '52706ff'},

    {'q': {'sequence': [{'sub-fen': 'rnbqkb1r/pp1p1ppp/4pn2/2pP4/2P5/2N5/PP2PPPP/R1BQKBNR'},
                        {'streak': [{'white-move': 'e5'}, {'black-move': 'dxe5'}, {'white-move': 'f5'},
                                    {'white-move': 'Ne4'}]}]},
     'sig': '97d170e'},

    {'q': {'sequence': [{'sub-fen': 'rnbqkb1r/pp1p1ppp/4pn2/2pP4/2P5/2N5/PP2PPPP/R1BQKBNR'},
                        {'streak': [{'white-move': 'e5'}, {'pass': ''}, {'white-move': 'f5'}]},
                        {'white-move': 'Ne4'}]},
     'sig': '52706ff'},
]


def signature(matches):
    d = str(matches)
    sha = hashlib.sha1(d).hexdigest()
    return sha[:7]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run a set of test queries')
    parser.add_argument('--pgn', default='../pgn/famous_games.pgn')
    parser.add_argument('--path', default='./scoutfish')
    parser.add_argument('--threads', default=1)
    args = parser.parse_args()

    if not os.path.isfile(args.path):
        print("File {} does not exsist".format(args.path))
        sys.exit(0)

    if not os.path.isfile(args.pgn):
        print("File {} does not exsist".format(args.pgn))
        sys.exit(0)

    # Spawn scoutfish
    p = Scoutfish(args.path)
    p.setoption('threads', args.threads)

    # Make DB
    sys.stdout.write('Making index...')
    sys.stdout.flush()
    p.open(args.pgn, True)
    print('done')

    # Run queries
    cnt = 1
    for e in QUERIES:
        sys.stdout.write('Query ' + str(cnt) + '...')
        sys.stdout.flush()
        result = p.scout(e['q'])
        num = str(result['match count'])
        if (e['sig'] == signature(result['matches'])):
            print('OK (' + num + ')')
        else:
            print('FAIL (' + num + ')')
        p.before = ''
        cnt += 1

    p.close()
