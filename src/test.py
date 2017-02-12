#!/usr/bin/env python

import os
import sys
import unittest

from scoutfish import Scoutfish


SCOUTFISH = './scoutfish.exe' if 'nt' in os.name else './scoutfish'

QUERIES = [
    {'q': {'sub-fen': 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR', 'stm': 'white'},
        'count': 501, 'matches': [{'ofs': 0, 'ply': [0]}, {'ofs': 666, 'ply': [0]}]},

    {'q': {'sub-fen': 'rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR', 'stm': 'balck'},
        'count': 229, 'matches': [{'ofs': 666, 'ply': [1]}, {'ofs': 2008, 'ply': [1]}]},

    {'q': {'sub-fen': '8/8/p7/8/8/1B3N2/8/8'},
        'count': 29, 'matches': [{'ofs': 90547, 'ply': [11]}, {'ofs': 106231, 'ply': [13]}]},

    {'q': {'sub-fen': '8/8/8/8/1k6/8/8/8', 'result': '1/2-1/2'},
        'count': 1, 'matches': [{'ofs': 810760, 'ply': [98]}]},

    {'q': {'sub-fen': ['8/8/8/q7/8/8/8/8', '8/8/8/r7/8/8/8/8']},
        'count': 72, 'matches': [{'ofs': 42247, 'ply': [6]}, {'ofs': 45161, 'ply': [34]}]},

    {'q': {'material': 'KQRRBNPPPPKQRRNNPPPP', 'stm': 'black'},
        'count': 2, 'matches': [{'ofs': 576617, 'ply': [49]}, {'ofs': 611693, 'ply': [43]}]},

    {'q': {'material': 'KQRRBNNPPPPKQRRBNNPPPP', 'result': '0-1'},
        'count': 2, 'matches': [{'ofs': 611693, 'ply': [39]}, {'ofs': 692493, 'ply': [60]}]},

    {'q': {'material': ['KRBPPPKRPPP', 'KRPPPKRPPP']},
        'count': 4, 'matches': [{'ofs': 666, 'ply': [77]}, {'ofs': 164246, 'ply': [83]}]},

    {'q': {'white-move': 'Nb7'},
        'count': 6, 'matches': [{'ofs': 141745, 'ply': [35]}, {'ofs': 538533, 'ply': [37]}]},

    {'q': {'black-move': 'c3'},
        'count': 27, 'matches': [{'ofs': 10226, 'ply': [34]}, {'ofs': 26360, 'ply': [8]}]},

    {'q': {'black-move': 'e1=Q'},
        'count': 1, 'matches': [{'ofs': 666, 'ply': [104]}]},

    {'q': {'black-move': 'O-O'},
        'count': 354, 'matches': [{'ofs': 0, 'ply': [16]}, {'ofs': 666, 'ply': [32]}]},

    {'q': {'skip': 200, 'limit': 100, 'black-move': 'O-O'},
        'count': 100, 'matches': [{'ofs': 485616, 'ply': [16]}, {'ofs': 487518, 'ply': [12]}]},

    {'q': {'black-move': 'O-O-O'},
        'count': 28, 'matches': [{'ofs': 10226, 'ply': [36]}, {'ofs': 64548, 'ply': [32]}]},

    {'q': {'black-move': ['O-O-O', 'O-O']},
        'count': 382, 'matches': [{'ofs': 0, 'ply': [16]}, {'ofs': 666, 'ply': [32]}]},


    {'q': {'white-move': ['a7', 'b7', 'Rac1']},
        'count': 120, 'matches': [{'ofs': 666, 'ply': [39]}, {'ofs': 2008, 'ply': [33]}, {'ofs': 10226, 'ply': [39]}]},

    {'q': {'imbalance': 'vPP'},
        'count': 52, 'matches': [{'ofs': 3313, 'ply': [12]}, {'ofs': 8436, 'ply': [12]}]},

    {'q': {'imbalance': ['BvN', 'NNvB']},
        'count': 142, 'matches': [{'ofs': 666, 'ply': [42]}, {'ofs': 16551, 'ply': [25]}]},


    {'q': {'moved': 'KP', 'captured': 'Q', 'result': ['1-0', '0-1']},
        'count': 48, 'matches': [{'ofs': 666, 'ply': [46]}, {'ofs': 8436, 'ply': [42]}]},

    {'q': {'result-type': 'mate', 'result': '0-1'},
        'count': 10, 'matches': [{'ofs': 11831, 'ply': [24]}, {'ofs': 30634, 'ply': [40]}]},

    {'q': {'sub-fen': ['rnbqkbnr/pp1p1ppp/2p5/4p3/3PP3/8/PPP2PPP/RNBQKBNR',
                       'rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R']},
        'count': 50, 'matches': [{'ofs': 16551, 'ply': [3]}, {'ofs': 75579, 'ply': [3]}]},

    {'q': {'sequence': [{'sub-fen': '8/3p4/8/8/8/8/8/8', 'result': '1-0'},
                        {'sub-fen': '8/2q5/8/8/8/8/8/R6R'}]},
        'count': 8, 'matches': [{'ofs': 195418, 'ply': [0, 42]}, {'ofs': 323394, 'ply': [0, 8]}]},

    {'q': {'sequence': [{'sub-fen': 'r1bqkb1r/pppp1ppp/2n2n2/1B2p3/'
                                    '4P3/2N2N2/PPPP1PPP/R1BQK2R'},
                        {'sub-fen': '8/8/8/8/2B5/8/8/8'},
                        {'sub-fen': '8/8/8/8/8/5B2/8/8'}]},
        'count': 2, 'matches': [{'ofs': 19722, 'ply': [7, 15, 21]}, {'ofs': 21321, 'ply': [7, 15, 21]}]},

    {'q': {'streak': [{'sub-fen': 'r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/2N2N2/PPPP1PPP/R1BQK2R'},
                      {'result': '0-1'}, {'result': '0-1'}]},
        'count': 2, 'matches': [{'ofs': 19722, 'ply': [7, 8, 9]}, {'ofs': 21321, 'ply': [7, 8, 9]}]},

    {'q': {'sequence': [{'sub-fen': 'rnbqkb1r/pp1p1ppp/4pn2/2pP4/2P5/2N5/PP2PPPP/R1BQKBNR'},
                        {'streak': [{'white-move': 'e5'}, {'black-move': 'dxe5'}, {'white-move': 'f5'}]},
                        {'white-move': 'Ne4'}]},
        'count': 1, 'matches': [{'ofs': 0, 'ply': [7, 37, 38, 39, 43]}]},

    {'q': {'sequence': [{'sub-fen': 'rnbqkb1r/pp1p1ppp/4pn2/2pP4/2P5/2N5/PP2PPPP/R1BQKBNR'},
                        {'streak': [{'white-move': 'e5'}, {'black-move': 'dxe5'}, {'white-move': 'f5'},
                                    {'white-move': 'Ne4'}]}]},
        'count': 0, 'matches': []},

    {'q': {'sequence': [{'sub-fen': 'rnbqkb1r/pp1p1ppp/4pn2/2pP4/2P5/2N5/PP2PPPP/R1BQKBNR'},
                        {'streak': [{'white-move': 'e5'}, {'pass': ''}, {'white-move': 'f5'}]},
                        {'white-move': 'Ne4'}]},
        'count': 1, 'matches': [{'ofs': 0, 'ply': [7, 37, 38, 39, 43]}]},

    {'q': {'streak': [{'imbalance': 'NNvB'}, {'imbalance': 'NNvB'}, {'imbalance': 'NNvB'}]},
        'count': 4, 'matches': [{'ofs': 82982, 'ply': [39, 40, 41]}, {'ofs': 99933, 'ply': [37, 38, 39]}]},

    {'q': {'streak': [{'moved': 'P', 'captured': 'Q'}, {'captured': ''}]},
        'count': 24, 'matches': [{'ofs': 19722, 'ply': [35, 36]}, {'ofs': 21321, 'ply': [35, 36]}]},

    {'q': {'white-move': 'e4', 'stm': 'balck', 'sub-fen': 'rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR'},
        'count': 229, 'matches': [{'ofs': 666, 'ply': [1]}]},
]


# Spawn scoutfish
sys.stdout.write('Making index...')
sys.stdout.flush()
p = Scoutfish(SCOUTFISH)
p.setoption('threads', 1)
p.open('../pgn/famous_games.pgn')
p.make()  # Force rebuilding of DB index
print('done')


class TestSuite(unittest.TestCase):
    ''' Each single test will be appended here as a new method
        with setattr(). The methods will then be loaded and
        run by unittest. '''
    pass


def create_test(expected):
    ''' Defines and returns a closure function that implements
        a single test. '''
    def test(self):
        result = p.scout(expected['q'])

        self.assertEqual(expected['count'], result['match count'])

        for idx, match in enumerate(expected['matches']):
            self.assertEqual(match['ofs'], result['matches'][idx]['ofs'])
            self.assertEqual(match['ply'], result['matches'][idx]['ply'])
    return test


for cnt, expected in enumerate(QUERIES):
    # Create a new test method
    test = create_test(expected)

    # Change it's name to be unique in TestSuite class
    test.__name__ = 'test_{num:02d}'.format(num=cnt + 1)

    # Add test to the TestSuite class
    setattr(TestSuite, test.__name__, test)


if __name__ == '__main__':
    unittest.main(verbosity=2)
    p.close()
