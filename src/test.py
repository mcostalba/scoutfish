#!/usr/bin/env python

import argparse
import os
import pexpect
import sys

QUERY_DB = [
    {'q': '{ "sub-fen": "8/8/p7/8/8/1B3N2/8/8" }',                                'matches': 29},
    {'q': '{ "sub-fen": "8/8/8/8/1k6/8/8/8", "result": "1/2-1/2" }',              'matches':  1},
    {'q': '{ "sub-fen": ["8/8/8/q7/8/8/8/8", "8/8/8/r7/8/8/8/8"] }',              'matches': 72},
    {'q': '{ "material": "KQRRBNPPPPKQRRNNPPPP", "stm": "BLACK" }',               'matches':  2},
    {'q': '{ "material": "KQRRBNNPPPPKQRRBNNPPPP", "result": "0-1" }',            'matches':  2},
    {'q': '{ "material": ["KRBPPPKRPPP", "KRPPPKRPPP"] }',                        'matches':  4},

    {'q': '{ "sub-fen": ["rnbqkbnr/pp1p1ppp/2p5/4p3/3PP3/8/PPP2PPP/RNBQKBNR", '
                        '"rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R"] }', 'matches': 50},

    {'q': '{ "sequence": [ { "sub-fen": "8/3p4/8/8/8/8/8/8" ,'
                            '"result": "1-0" },'
                          '{ "sub-fen": "8/2q5/8/8/8/8/8/R6R" }] }',              'matches':  8},

    {'q': '{ "sequence": [ { "sub-fen": "r1bqkb1r/pppp1ppp/2n2n2/1B2p3/'
                                        '4P3/2N2N2/PPPP1PPP/R1BQK2R" },'
                          '{ "sub-fen": "8/8/8/8/2B5/8/8/8" },'
                          '{ "sub-fen": "8/8/8/8/8/5B2/8/8" } ] }',               'matches':  2},
]


def run_queries(p, db):
    cmd = 'scout ' + db + ' '
    cnt = 1
    for e in QUERY_DB:
        sys.stdout.write('Query ' + str(cnt) + '...')
        sys.stdout.flush()
        p.sendline(cmd + e['q'])
        p.expect('matches')
        result = p.before
        result = result.split('"match count": ')[1].split(',')[0]
        if (int(result) == e['matches']):
            print('OK')
        else:
            print('FAIL')
        p.before = ''
        cnt += 1


def run_test(args):
    # Spawn scoutfish
    p = pexpect.spawn(args.path)
    p.sendline('setoption name threads value ' + str(args.threads))

    # Make DB
    db = os.path.splitext(args.pgn)[0]+'.bin'
    sys.stdout.write("Making DB '" + os.path.basename(db) + "'...")
    sys.stdout.flush()
    p.sendline('make ' + args.pgn)
    p.sendline('isready')
    p.expect('readyok')
    p.before = ''
    print('done')

    # Run queries
    run_queries(p, db)

    # Terminate scoutfish
    p.sendline('quit')
    p.expect(pexpect.EOF)


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

    run_test(args)
