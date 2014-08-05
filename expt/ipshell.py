#!/usr/bin/env python

from IPython import embed
from xprmntsdb import XprmntsDB
from plot import Plot

db = XprmntsDB()
plt = Plot(db)
embed()