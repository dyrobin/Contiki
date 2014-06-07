#! /usr/bin/python

import sys, os, database as db, numpy as np

# The experiment data contains 8 metrics and each of them is measured by 
# several times. The result is a tuple.
def load_result_from_file(path, filename):
	tfcintvl, rxpct, tpdusize, datasize = filename.split("_")[0:4]
	items = []
	with open(path + filename) as f:
		for line in f:
			if "Statistics" in line:
				# load packets, retrans and loss
				tmp = f.next().split()[1::2]
				items.extend([tmp[0], tmp[1], tmp[2].strip("%")])
				# load fragmts, frames and bytes
				tmp = f.next().split()[1::2]
				items.extend([tmp[0], tmp[1], tmp[2]])
				# load time, dtime
				tmp = f.next().split()[1:3]
				items.extend([tmp[0], tmp[1].strip("()")])
	# items might be empty
	if items:
		exprdata = np.array(items).reshape((-1, 8))
		return (tfcintvl, rxpct, tpdusize, datasize, exprdata)
	else:
		return (tfcintvl, rxpct, tpdusize, datasize, None)


if len(sys.argv) != 2:
	print "Usage: {} pattern".format(sys.argv[0])
	sys.exit(0)

# scan all *.log in path that starts with pattern
# then prepare the results to be saved into database
pattern = sys.argv[1]
path = "logs/"
logs = [f for f in os.listdir(path) \
			if f.endswith(".log") and f.startswith(pattern)]

if logs:
	results = [load_result_from_file(path, log) for log in logs]
	#  save results in database
	dbname = "rslts.db"
	if db.create(dbname) and db.insert(dbname, results):
		print "Experiment results are loaded successfully."
else:
	print "Error: No file matches", pattern
