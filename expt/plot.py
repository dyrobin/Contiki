#! /usr/bin/python
import sys
import database as db, numpy as np
import matplotlib.pyplot as plt


if len(sys.argv) != 3:
	print "Usage: {} INTVL RX".format(sys.argv[0])
	sys.exit(0)

intvl = sys.argv[1]
rx = sys.argv[2]

dbname = "rslts.db"

sizes = sorted(set([rsl[3] for rsl in db.select(dbname, intvl, rx)]))

metrics = {"packets": 0, "retrans": 1, "loss(%)": 2, "frags": 3, \
		   "frames": 4, "bytes": 5, "time1(ms)": 6, "time2(ms)": 7}


f, axes = plt.subplots(4, 2, sharex=True)

for axe in axes:
	for sz in sizes:
		data = []
		for rsl in db.select(dbname, intvl, rx, -1, sz):
			if rsl[4] is not None:
				sample = rsl[4][:,index].astype(np.float)
				if (rsl[4].shape[0] != 10):
					func = np.mean(sample) * 1.2 * 10 / rsl[4].shape[0]
				else:
					func = np.mean(sample)
				data.append([rsl[2], np.mean(sample), np.std(sample), \
							np.amin(sample), np.amax(sample), func, ])
			else:
				data.append([rsl[2], np.nan, np.nan, np.nan, np.nan])

		data.sort(key=lambda row: row[0])
		data = np.array(data)

		if metric == "func":
			plt.plot(data[:,0], data[:,5], "-o", label="size {}".format(sz))
		else:
#			plt.errorbar(data[:,0], data[:,1], data[:,2], fmt=None)
			plt.plot(data[:,0], data[:,1], "-o", label="size {}".format(sz))




for metric in metrics:
	index = metrics.index(metric)
	plt.subplot(4, 2, index + 1)

	for sz in sizes:
		data = []
		for rsl in db.select(dbname, rx, -1, sz):
			if rsl[3] is not None:
				tmp = rsl[3].shape
				sample = rsl[3][:,index].astype(np.float)
				if (tmp[0] != 10):
					func = np.mean(sample) * 1.2 * 10 / tmp[0]
				else:
					func = np.mean(sample)
				data.append([rsl[1], np.mean(sample), np.std(sample), \
							np.amin(sample), np.amax(sample), func])
			else:
				data.append([rsl[1], np.nan, np.nan, np.nan, np.nan])

		data.sort(key=lambda row: row[0])
		data = np.array(data)

		if metric == "func":
			plt.plot(data[:,0], data[:,5], "-o", label="size {}".format(sz))
		else:
#			plt.errorbar(data[:,0], data[:,1], data[:,2], fmt=None)
			plt.plot(data[:,0], data[:,1], "-o", label="size {}".format(sz))

	plt.xticks(data[:,0])
	plt.xlabel("TPDU (bytes)")
	plt.ylabel(metric)
#	plt.legend()


plt.show()