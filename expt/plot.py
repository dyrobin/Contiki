#!/usr/bin/env python

import numpy as np
import matplotlib.pyplot as plt
from xprmntsdb import XprmntsDB

class Plot:

	metrics = ["packets", "retrans", "loss(%)", "frags", \
		   	   "frames", "bytes", "time(ms)", "time2(ms)"]

	def __init__(self, db):
		if not isinstance(db, XprmntsDB):
			raise ValueError(
				"It requires XprmntsDB but '{}' is passed in.".format(type(db)))
		self.db = db


	def plot1(self, datasize=1024, metric=6):
		dic = self.db.get_colvals()
		tfcintvls = dic["tfcintvl"]
		rxratios = dic["rxratio"]
		if not tfcintvls or not rxratios:
			raise ValueError("Data is not complete yet.")

		i, j = 0, 0
		fig, axes = plt.subplots(len(tfcintvls), len(rxratios), 
								 sharex=True)
		fig.suptitle("Fig: Time vs TPDU")
		for tfcintvl in tfcintvls:
			for rxratio in rxratios:
				xy = []
				for row in self.db.retrieve_data(tfcintvl, rxratio, -1, datasize):
					if row[4] is not None:
						successes, _ = row[4].shape
						sample = row[4][:, metric].astype(np.float)
						if (successes != 10):
							atime = np.nanmean(sample) * 1.2 * 10 / successes
						else:
							atime = np.nanmean(sample)
						xy.append([row[2], atime, np.nanstd(sample), successes])
					else:
						xy.append([row[2], np.nan, np.nan, np.nan])
				
				xy.sort(key=lambda item: item[0])
				xy = np.array(xy)

				ax = axes[i][j]
				ax.plot(xy[:, 0], xy[:, 1], "-o", 
							label="size {}".format(datasize))
				ax.set_xticks(xy[:, 0])
#				ax.set_xlabel("TPDU (bytes)")
#				ax.set_ylabel(Plot.metrics[metric])
				ax.set_title("tfcintvl={} rxratio={}".format(tfcintvl, rxratio))

				axtw = ax.twinx()
				axtw.plot(xy[:, 0], xy[:, 3], "r")
				axtw.set_ylim(0, 11)

				j += 1
			i += 1
			j = 0
		plt.show()

	def plot2(self, datasize=1024):
		dic = self.db.get_colvals()
		tfcintvls = dic["tfcintvl"]
		rxratios = dic["rxratio"]
		if not tfcintvls or not rxratios:
			raise ValueError("Data is not complete yet.")


		for tfcintvl in tfcintvls:
			for rxratio in rxratios:
				for row in self.db.retrieve_data(tfcintvl, rxratio, -1, datasize):
					if row[2] == 0: 
						continue
					print "{}_{}_{}: ".format(tfcintvl, rxratio, row[2])
					if row[4] is not None:
						x = row[4][:, 1].astype(np.float)
						y = row[4][:, 6].astype(np.float)
#						print "\t", x
#						print "\t", y 
						try:
							coef = np.polyfit(x, y, 1)
						except ValueError as e:
							if np.allclose(x, np.zeros(x.shape)):
								coef = [0, np.mean(y)]
							else:
								raise e
						print "\t", coef


		return

		i, j = 0, 0
		fig, axes = plt.subplots(len(tfcintvls), len(rxratios))
		fig.suptitle("Fig: Time vs Retrans")
		for tfcintvl in tfcintvls:
			for rxratio in rxratios:
				ax = axes[i][j]

				for row in self.db.retrieve_data(tfcintvl, rxratio, -1, datasize):
					if row[4] is not None:
						x = row[4][:, 1]
						y = row[4][:, 6]
					else:
						x = []
						y = []
					ax.plot(x, y, "o")
				
				ax.set_title("tfcintvl={} rxratio={}".format(tfcintvl, rxratio))

				j += 1
			i += 1
			j = 0
		plt.show()