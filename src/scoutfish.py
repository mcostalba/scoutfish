#!/usr/bin/env python

import json
import os
import pexpect

from pexpect.popen_spawn import PopenSpawn


class Scoutfish:
    def __init__(self, engine):
        self.p = PopenSpawn(engine, encoding="utf-8")
        self.wait_ready()

    def wait_ready(self):
        self.p.sendline('isready')
        self.p.expect(u'readyok')

    def close(self):
        '''Terminate scoutfish. Not really needed: engine will terminate as
           soon as pipe is closed, i.e. when we exit.'''
        self.p.sendline('quit')
        self.p.expect(pexpect.EOF)

    def make(self, pgn):
        '''Make an index out of a pgn file. Needed before to run queries'''
        self.p.sendline('make ' + pgn)
        self.wait_ready()
        self.db = self.p.before.split('DB file:')[1]
        self.db = self.db.split()[0]
        return self.db

    def setoption(self, name, value):
        '''Set an option value, like threads number'''
        self.p.sendline('setoption name ' + name + ' value ' + str(value))
        self.wait_ready()

    def scout(self, q, db=''):
        '''Run a query in JSON format. The result will be in JSON format too'''
        if not db:
            db = self.db
        cmd = 'scout ' + db + ' ' + q
        self.p.sendline(cmd)
        self.wait_ready()
        result = json.loads(self.p.before)
        self.p.before = ''
        return result
