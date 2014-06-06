#! /usr/bin/python
import sys
import database as db, numpy as np
import matplotlib.pyplot as plt


if len(sys.argv) != 2:
	print "Usage: {} rx".format(sys.argv[0])
	sys.exit(0)

rx = sys.argv[1]


dbname="rslts.db"

sizes = sorted(set([rsl[2] for rsl in db.select(dbname, rx)]))

metrics = ["Packets", "retrans", "loss", "fragmts", "frames", "bytes", \
			"time1(ms)", "time2(ms)"]

for metric in metrics:
	index = metrics.index(metric)
	plt.subplot(2, 4, index + 1)

	for sz in sizes:
		data = []
		for rsl in db.select(dbname, rx, -1, sz):
			if rsl[3] is not None:
				sample = rsl[3][:,index].astype(np.float)
				data.append([rsl[1], np.mean(sample), np.std(sample), \
							np.amin(sample), np.amax(sample)])
			else:
				data.append([rsl[1], np.nan, np.nan, np.nan, np.nan])

		data.sort(key=lambda row: row[0])
		data = np.array(data)

	#	plt.errorbar(data[:,0], data[:,1], data[:,2], fmt=None)
		plt.plot(data[:,0], data[:,1], "-o", label="size {}".format(sz))

	plt.xticks(data[:,0])
	plt.xlabel("TPDU (bytes)")
	plt.ylabel(metric)
#	plt.legend()


plt.show()