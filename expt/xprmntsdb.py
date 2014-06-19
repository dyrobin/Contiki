# database module: save experiemnt results into sqlite database. The type of 
# result is numpy.ndarray and corresponding adaptor/convertor is provided as 
# well.
# Authored by Yang Deng <yang.deng@aalto.fi>

import io, os
import sqlite3 as db, numpy as np

class XprmntsDB:

	# pickle ndarray into sqlite
	def __adapt_ndarray(array):
		tmp = io.BytesIO()
		np.save(tmp, array)
		buf = buffer(tmp.getvalue())
		tmp.close()
		return buf

	# unpickle ndarray from sqlite
	def __convert_ndarray(text):
		tmp = io.BytesIO(text)
		array = np.load(tmp)
		tmp.close()
		return array

	db.register_adapter(np.ndarray, __adapt_ndarray)
	db.register_converter("ndarray", __convert_ndarray)

	def __init__(self, name):
		self.name = name

	# Several database operations
	# Using "with" clause: If success, conn.commit() is called automatically
	# Else conn.rollback() is called automatically
	def create(self):
		conn = db.connect(self.name, detect_types=db.PARSE_DECLTYPES) 
		try:
			with conn:
				conn.execute("CREATE TABLE IF NOT EXISTS xprmnts ( \
					tfcintvl INTEGER NOT NULL, \
					rxpct INTEGER NOT NULL, \
					tpdusize INTEGER NOT NULL, \
					datasize INTEGER NOT NULL, \
					exprdata ndarray, \
					PRIMARY KEY (tfcintvl, rxpct, tpdusize, datasize))")
			return True
		except db.Error as e:
			print "Create Error:", e
			return False
		finally:
			conn.close()

	def insert(self, results):
		if not os.path.isfile(self.name):
			print "Insert Error: {} doesn't exist.".format(self.name)
			return False
		conn = db.connect(self.name, detect_types=db.PARSE_DECLTYPES)
		try:
			with conn:
				conn.executemany("INSERT INTO xprmnts VALUES (?, ?, ?, ?, ?)", results)
			return True
		except db.Error as e:
			print "Insert Error:", e
			return False
		finally:
			conn.close()

	def select(self, tfcintvl=-1, rxpct=-1, tpdusize=-1, datasize=-1):
		if not os.path.isfile(self.name):
			print "Select Error: {} doesn't exist.".format(self.name)
			return None
		# form sql query according to arguments
		sql = "SELECT * FROM xprmnts"

		conds = []
		if tfcintvl != -1:
			conds.append("tfcintvl={}".format(tfcintvl))
		if rxpct != -1:
			conds.append("rxpct={}".format(rxpct))
		if tpdusize != -1:
			conds.append("tpdusize={}".format(tpdusize))
		if datasize != -1:
			conds.append("datasize={}".format(datasize))
		if conds:
			sql += " WHERE " + " AND ".join(conds)

		# select values
		conn = db.connect(self.name, detect_types=db.PARSE_DECLTYPES)
		try:
			with conn:
				c = conn.cursor()
				c.execute(sql)
				exprs = c.fetchall()
			return exprs
		except db.Error as e:
			print "Select Error:", e
			return None
		finally:
			conn.close()

	def remove(self):
		if os.path.isfile(self.name):
			os.remove(self.name)

# For test
if __name__ == "__main__":
	data = []
	
	for i in range(0, 3):
		for j in range(0, 3):
			for k in range(0, 3):
				for l in range(0, 2):
					# 3-dimensional array
					t = np.ones((2, 2, 2))
					t[0][0][0], t[0][0][1] = 0, 1
					t[0][1][0], t[0][1][1] = 2, 3
					t[1][0][0], t[1][0][1] = 4, 5
					t[1][1][0], t[1][1][1] = 6, 7
					data.append((i, j, k, l, t))

	xdb = XprmntsDB("test.db")
	if xdb.create() and xdb.insert(data):
		print "*******************"
		for row in xdb.select():
			print row

		print "*******************"
		for row in xdb.select(-1, 2, -1, 0):
			print row

		print "*******************"
		for row in xdb.select(-1, -1, -1, 0):
			print row

	xdb.remove()