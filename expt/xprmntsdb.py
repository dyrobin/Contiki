# database module: save experiemnt results into sqlite database. The type of 
# result is numpy.ndarray and corresponding adaptor/convertor is provided as 
# well.
# Authored by Yang Deng <yang.deng@aalto.fi>

import io, os, re
import sqlite3 as db, numpy as np

class XprmntsDB:
	@staticmethod
	def __register_ndarray_type():
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
		# register
		db.register_adapter(np.ndarray, __adapt_ndarray)
		db.register_converter("ndarray", __convert_ndarray)

	# create a database if not exist
	def __init__(self, name="xprmnts.db"):
		if not os.path.isfile(name):
			conn = db.connect(name, detect_types=db.PARSE_DECLTYPES)
			with conn:
				conn.execute("CREATE TABLE xprmnts ( \
					tfcintvl INTEGER NOT NULL, \
					rxpct INTEGER NOT NULL, \
					tpdusize INTEGER NOT NULL, \
					datasize INTEGER NOT NULL, \
					exprdata ndarray, \
					PRIMARY KEY (tfcintvl, rxpct, tpdusize, datasize))")
		else:
			conn = db.connect(name, detect_types=db.PARSE_DECLTYPES)
		self.conn = conn

	def __del__(self):
		self.conn.close()

	def load_from_file(self, path, overwrite=False):
		if not os.path.isfile(path):
			raise ValueError("'{}' is not a file.".format(path))
		filename = os.path.basename(path)
			
		if not re.match(r"(\d+_){4}.+\.log", filename):
			raise ValueError("'{}' has a wrong name.".format(filename))
		tfcintvl, rxpct, tpdusize, datasize = filename.split("_")[0:4]
		
		metrics = []
		with open(path) as f:
			for line in f:
				if "Statistics" in line:
					# load packets, retrans and loss
					tmp = f.next().split()[1::2]
					metrics.extend([tmp[0], tmp[1], tmp[2].strip("%")])
					# load fragmts, frames and bytes
					tmp = f.next().split()[1::2]
					metrics.extend([tmp[0], tmp[1], tmp[2]])
					# load time, dtime
					tmp = f.next().split()[1:3]
					metrics.extend([tmp[0], tmp[1].strip("()")])

		
		key = (tfcintvl, rxpct, tpdusize, datasize)
		# metrics might be empty
		if metrics:
			row = key + (np.array(metrics).reshape((-1, 8)),)
		else:
			row = key + (None,)

		try:
			with self.conn:
				self.conn.execute(
					"INSERT INTO xprmnts VALUES (?, ?, ?, ?, ?)", row)
		except Exception as e:
			print e
			if overwrite:
				with self.conn:
					self.conn.execute(
						"REPLACE INTO xprmnts VALUES (?, ?, ?, ?, ?)", row)

	def load_from_directory(self, path, filter=None, overwrite=False):
		if not os.path.isdir(path):
			raise ValueError("'{}' is not a directory.".format(path))

		files = [f for f in os.listdir(path) if os.path.isfile]
		if filter:
			files = 
		else:
			

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

	# static initialization for class
	__register_ndarray_type()

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