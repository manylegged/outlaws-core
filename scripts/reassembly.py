#!/usr/bin/python
##################################################################################
# Reassembly file format parsing sample script
#
# validates reassembly format data files (.lua)
# also loads agent files and prints the P of the largest ship
#
# Reassembly itself actually gives better error messages than this script - check the log!
#

import sys
import os
import gzip
import getopt
import urllib2
import traceback
from slpp import slpp as lua

# This file contains information about all the blocks in the game
BLOCKSTATS_URL = "http://www.anisopteragames.com/sync/blockstats.lua"
BLOCKSTATS = "blockstats.lua"

g_blockstats = {}


####################################################################################################

def get_all_ships(save):
    if "blueprint" in save:
        if save["blueprint"]:
            yield save["blueprint"]
    if "children" in save:
        for ch in save["children"]:
            if ch:
                yield ch
    if "blueprints" in save:
        for ch in save["blueprints"]:
            if ch:
                yield ch
            

class agent_summary:
    def __init__(self):
        self.name = ""
        self.points = 0
        self.error = None
        self.mods = False
    pass

def load_stats():
    if not g_blockstats:
        if os.path.exists(BLOCKSTATS):
            print "Using local blockstats.lua"
            stats = lua.decode(open(BLOCKSTATS).read())
        else:
            print "Fetching blockstats.lua from server"
            stats = lua.decode(urllib2.urlopen(BLOCKSTATS_URL).read())
        for bl in stats:
            g_blockstats[bl["ident"]] = bl
    return g_blockstats

def decode_agent(data, ptrace=False):
    stats = load_stats()
    ag = agent_summary()
    try:
        if isinstance(data, str):
            data = data.decode("utf-8")
        save = lua.decode(data)
        if isinstance(save, basestring):
            raise TypeError("decode failed, got str#%d" % len(save))
        elif not isinstance(save, dict):
            raise TypeError("decode failed, got '%s'" % save.__class__.__name__)
        ag.name = save["name"]
        for bp in get_all_ships(save):
            bp["data"]
            points = 0
            count = 0
            for bl in bp["blocks"]:
                ident = bl if isinstance(bl, int) else bl[0]
                if (ident >= 0xffff):
                    continue
                if ident == 833:
                    ag.error = "contains Station Command block 833"
                    return ag
                points += stats[ident].get("deadliness", 0)
                count += 1
            ag.points = max(ag.points, points)
            if count > kConstructorBlockLimit:
                ag.error = "too many blocks (%d is over max)" % count
                return ag
            if points > kPointMax:
                ag.mods = True
        if ag.points == 0 or len(list(saved_blueprints(save))) == 0:
            ag.error = "empty fleet"
        if "mods" in save:
            ag.mods = True
    # except UnicodeDecodeError:
        # raise
    except Exception as e:
        if ptrace:
            ag.error = traceback.format_exc()
        else:
            ag.error = "%s(%s)" % (e.__class__.__name__, str(e))
    return ag


def read_file(path):
    try:
        fil = gzip.open(path) if path.endswith(".gz") else open(path)
        dat = fil.read()
        fil.close()
    except Exception as e:
        print "FAILED read %s, %s" % (path, e)
        return None
    return dat


def help():
    print """
%s [-ah] <file> [files]

Reassembly data file parser script. Reads and validates arguments as .lua Reassembly data files.
  -h  print this message
  -a  read and validate agent/blueprint files instead
""" % sys.argv[0]
    exit(0)

    
if __name__ == '__main__':
    options, args = getopt.getopt(sys.argv[1:], "ha")
    agents = False
    for (opt, val) in options:
        if opt == "-h":
            help()
        if opt == "-a":
            agents = True
    if len(args) == 0:
        help()
    for path in args:
        dat = read_file(path)
        if agents:
            ag = decode_agent(dat)
            if not ag.error:
                print "OK %dP: %s" % (ag.points, path)
            else:
                print "FAILED %s: %s" % (ag.error, path)
        else:
            dec = None
            error = ""
            try:
                dec = lua.decode(dat)
            except Exception as e:
                error = str(e)
            if dec:
                print "OK: %s" % path
            else:
                print "FAILED (%s): %s" % (error, path)
