#!/usr/bin/env python

import json
import os
import pexpect
import sys

from pexpect.popen_spawn import PopenSpawn


class Scoutfish:
    def __init__(self, engine = ''):
        if not engine:
            engine = './scoutfish'
        self.p = PopenSpawn(engine, encoding="utf-8")
        self.wait_ready()
        self.pgn = ''
        self.db = ''

    def wait_ready(self):
        self.p.sendline('isready')
        self.p.expect(u'readyok')

    def open(self, pgn, force_make=False):
        '''Open a PGN file and create an index if not exsisting or if force_make
           is set.'''
        if not os.path.isfile(pgn):
            print("File {} does not exsist".format(pgn))
            sys.exit(0)
        self.pgn = pgn
        self.db = os.path.splitext(pgn)[0] + '.scout'
        if force_make or not os.path.isfile(self.db):
            self.db = self.make(pgn)

    def close(self):
        '''Terminate scoutfish. Not really needed: engine will terminate as
           soon as pipe is closed, i.e. when we exit.'''
        self.p.sendline('quit')
        self.p.expect(pexpect.EOF)
        self.pgn = ''
        self.db = ''

    def make(self, pgn):
        '''Make an index out of a pgn file. Normally called by open()'''
        self.p.sendline('make ' + pgn)
        self.wait_ready()
        db = self.p.before.split('DB file:')[1]
        return db.split()[0]

    def setoption(self, name, value):
        '''Set an option value, like threads number'''
        self.p.sendline('setoption name ' + name + ' value ' + str(value))
        self.wait_ready()

    def scout(self, q):
        '''Run a query in JSON format. The result will be in JSON format too'''
        if not self.db:
            print("Unknown DB, first open a PGN file")
            sys.exit(0)
        cmd = 'scout ' + self.db + ' ' + str(q)
        self.p.sendline(cmd)
        self.wait_ready()
        result = json.loads(self.p.before)
        self.p.before = ''
        return result


    def get_games(self, list):
        '''Retrieve the PGN games specified in the list accessing the PGN file.
           Games are added to each match entry with a new key "pgn" '''
        if not self.pgn:
            print("Unknown DB, first open a PGN file")
            sys.exit(0)

        with open(self.pgn, "r") as f:
            for m in list:
                ofs = m['ofs']
                f.seek(ofs)
                game = ''
                for line in f:
                    if "[Event" in line and game.strip():
                        break # Next game start
                    game += line
                m['pgn'] = game.strip()
        return list
