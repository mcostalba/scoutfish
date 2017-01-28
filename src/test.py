#!/usr/bin/env python

import argparse
import hashlib
import json
import os
import sys
from scoutfish import Scoutfish

SCOUTFISH = './scoutfish.exe' if 'nt' in os.name else './scoutfish'

QUERIES = [
    {'q': {'sub-fen': 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR', 'stm': 'white'}, 'sig': 'bbba013'},
    {'q': {'sub-fen': 'rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR', 'stm': 'black'}, 'sig': 'cbfff97'},
    {'q': {'sub-fen': '8/8/p7/8/8/1B3N2/8/8'}, 'sig': '27e9564'},
    {'q': {'sub-fen': '8/8/8/8/1k6/8/8/8', 'result': '1/2-1/2'}, 'sig': 'acc1e8a'},
    {'q': {'sub-fen': ['8/8/8/q7/8/8/8/8', '8/8/8/r7/8/8/8/8']}, 'sig': '75ed0d6'},
    {'q': {'material': 'KQRRBNPPPPKQRRNNPPPP', 'stm': 'BLACK'}, 'sig': '6018fa4'},
    {'q': {'material': 'KQRRBNNPPPPKQRRBNNPPPP', 'result': '0-1'}, 'sig': '2cd1c3e'},
    {'q': {'material': ['KRBPPPKRPPP', 'KRPPPKRPPP']}, 'sig': '9cf980c'},
    {'q': {'white-move': 'Nb7'}, 'sig': '1683032'},
    {'q': {'black-move': 'c3'}, 'sig': 'e4f532e'},
    {'q': {'black-move': 'e1=Q'}, 'sig': '89abcc3'},
    {'q': {'black-move': 'O-O'}, 'sig': '423497f'},
    {'q': {'skip': 200, 'limit': 100, 'black-move': 'O-O'}, 'sig': 'bcd9d2c'},
    {'q': {'black-move': 'O-O-O'}, 'sig': '8072796'},
    {'q': {'black-move': ['O-O-O', 'O-O']}, 'sig': '99f2b6f'},
    {'q': {'white-move': ['a7', 'b7']}, 'sig': 'fe84899'},
    {'q': {'imbalance': 'vPP'}, 'sig': '4eeb4cf'},
    {'q': {'imbalance': ['BvN', 'NNvB']}, 'sig': '07318ef'},
    {'q': {'moved': 'KP', 'captured': 'Q'}, 'sig': 'e3f73e8'},
    {'q': {'result-type': 'mate', 'result': '0-1'}, 'sig': '87f61c4'},

    {'q': {'sub-fen': ['rnbqkbnr/pp1p1ppp/2p5/4p3/3PP3/8/PPP2PPP/RNBQKBNR',
                       'rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R']},
     'sig': '826e5b5'},

    {'q': {'sequence': [{'sub-fen': '8/3p4/8/8/8/8/8/8', 'result': '1-0'},
                        {'sub-fen': '8/2q5/8/8/8/8/8/R6R'}]},
     'sig': '0f061d8'},

    {'q': {'sequence': [{'sub-fen': 'r1bqkb1r/pppp1ppp/2n2n2/1B2p3/'
                                    '4P3/2N2N2/PPPP1PPP/R1BQK2R'},
                        {'sub-fen': '8/8/8/8/2B5/8/8/8'},
                        {'sub-fen': '8/8/8/8/8/5B2/8/8'}]},
     'sig': '104d885'},

    {'q': {'streak': [{'sub-fen': 'r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/2N2N2/PPPP1PPP/R1BQK2R'},
                      {'result': '0-1'}, {'result': '0-1'}]},
     'sig': 'b59334d'},

    {'q': {'sequence': [{'sub-fen': 'rnbqkb1r/pp1p1ppp/4pn2/2pP4/2P5/2N5/PP2PPPP/R1BQKBNR'},
                        {'streak': [{'white-move': 'e5'}, {'black-move': 'dxe5'}, {'white-move': 'f5'}]},
                        {'white-move': 'Ne4'}]},
     'sig': '7ea196f'},

    {'q': {'sequence': [{'sub-fen': 'rnbqkb1r/pp1p1ppp/4pn2/2pP4/2P5/2N5/PP2PPPP/R1BQKBNR'},
                        {'streak': [{'white-move': 'e5'}, {'black-move': 'dxe5'}, {'white-move': 'f5'},
                                    {'white-move': 'Ne4'}]}]},
     'sig': 'cab98f3'},

    {'q': {'sequence': [{'sub-fen': 'rnbqkb1r/pp1p1ppp/4pn2/2pP4/2P5/2N5/PP2PPPP/R1BQKBNR'},
                        {'streak': [{'white-move': 'e5'}, {'pass': ''}, {'white-move': 'f5'}]},
                        {'white-move': 'Ne4'}]},
     'sig': '7ea196f'},

    {'q': {'streak': [{'imbalance': 'NNvB'}, {'imbalance': 'NNvB'}, {'imbalance': 'NNvB'}]},
     'sig': '84d67bf'},

    {'q': {'streak': [{'moved': 'P', 'captured': 'Q'}, {'captured': ''}]},
     'sig': 'd431d7b'},
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
