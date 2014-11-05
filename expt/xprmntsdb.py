#!/usr/bin/env python
"""
Experiments Database module
Authored by Yang Deng <yang.deng@aalto.fi>
"""

import io, os, re
import sqlite3 as db, numpy as np

class XprmntsDB:
    """A database used to store experiments data. The data are stored in sqlite 
    and accessed by sqlite3 DB-API. The database contains only one table 
    (xprmnts) whose schema is as follows:
        tfcintvl | rxratio | tpdusize | datasize | exprdata
    The first four columns form a primary key and the type of theirs is integer;
    the last column is the experiemnt data whose type is numpy.ndarray.
    """

    def __register_ndarray_type():
        """Provide the adapter and converter for numpy.ndarray and then register
        them with sqlite. This method is called when class(not instance) 
        initialization.
        """
        # pickle ndarray into sqlite
        def db_adapt_ndarray(array):
            bytesio = io.BytesIO()
            np.save(bytesio, array)
            buf = buffer(bytesio.getvalue())
            bytesio.close()
            return buf
        # unpickle ndarray from sqlite
        def db_convert_ndarray(text):
            bytesio = io.BytesIO(text)
            array = np.load(bytesio)
            bytesio.close()
            return array
        # register
        db.register_adapter(np.ndarray, db_adapt_ndarray)
        db.register_converter("ndarray", db_convert_ndarray)

    # static initialization for class
    __register_ndarray_type()


    # Note: Using "with" clause if success, conn.commit() is called 
    # automatically else conn.rollback() is called automatically
    def __init__(self, name="xprmnts.db"):
        """When an instance is initialized, open an existed database otherwise 
        create a new one. The connection to the database is stored by instance.
        """
        if not os.path.isfile(name):
            conn = db.connect(name, detect_types=db.PARSE_DECLTYPES)
            with conn:
                conn.execute("CREATE TABLE xprmnts ( \
                    tfcintvl INTEGER NOT NULL, \
                    rxratio INTEGER NOT NULL, \
                    tpdusize INTEGER NOT NULL, \
                    datasize INTEGER NOT NULL, \
                    exprdata ndarray, \
                    PRIMARY KEY (tfcintvl, rxratio, tpdusize, datasize))")
        else:
            conn = db.connect(name, detect_types=db.PARSE_DECLTYPES)
        self.conn = conn


    def __del__(self):
        """Close the stored connection when instance is revoked."""
        self.conn.close()


    def insert_row(self, row, mode=0):
        """Insert a row into the database. If conflict occurs, different things
        will be done depending on the value of mode.
            0: Ignore mode, new data will be ignored.
            1: Replace mode, new data will replace old data.
            2: Merge mode, new data will be merged with old data.
        """
        try:
            with self.conn:
                self.conn.execute(
                    "INSERT INTO xprmnts VALUES (?, ?, ?, ?, ?)", row)
        except db.IntegrityError:
            # handle conflict issues
            msg = "Conflicted: new " + str(row[:-1])
            if mode == 0:
                msg += " was ignored."
            elif mode == 1:
                with self.conn:
                    self.conn.execute(
                        "REPLACE INTO xprmnts VALUES (?, ?, ?, ?, ?)", row)
                msg += " replaced old one."
            elif mode == 2:
                old = self.get_data(row[0], row[1], row[2], row[3])[0][4]
                new = row[4]
                # handle None specifically
                if old is not None:
                    if new is None:
                        new = old
                    else:
                        new = np.concatenate((old, new))

                row = row[0:4] + (new, )
                with self.conn:
                    self.conn.execute(
                        "REPLACE INTO xprmnts VALUES (?, ?, ?, ?, ?)", row)
                msg += " was merged with old one."
            else:
                msg += " was ignored (unknown mode)."
            print msg


    def get_data(self, tfcintvl=-1, rxratio=-1, tpdusize=-1, datasize=-1):
        """Get data by the value of columns. If the value is -1, all values
        of that column will be retrieved.
        """
        # form sql query according to arguments
        sql = "SELECT * FROM xprmnts"
        conds = []
        if tfcintvl != -1:
            conds.append("tfcintvl={}".format(tfcintvl))
        if rxratio != -1:
            conds.append("rxratio={}".format(rxratio))
        if tpdusize != -1:
            conds.append("tpdusize={}".format(tpdusize))
        if datasize != -1:
            conds.append("datasize={}".format(datasize))
        if conds:
            sql += " WHERE " + " AND ".join(conds)

        # select values
        with self.conn:
            c = self.conn.cursor()
            c.execute(sql)
            data = c.fetchall()
        return data


    def get_failed(self):
        """Get failed experiments, a list of tuples (tfcintvl, rxratio, tpdusize, 
        datasize) is returned if exists.
        """
        with self.conn:
            c = self.conn.cursor()
            c.execute("SELECT * FROM xprmnts WHERE exprdata IS NULL")
            data = c.fetchall()
        return data


    def print_data(self, tfcintvl=-1, rxratio=-1, tpdusize=-1, datasize=-1):
        """Print data retrieved by the value of columns."""
        data = self.get_data(tfcintvl, rxratio, tpdusize, datasize)
        if data:
            for row in data:
                print row[0:4]
                print "\t", str(row[4]).replace("\n", "\n\t")
        else:
            print None


    def get_colvals(self):
        """Get all possible values for columns that form primary key. It returns
        a dictionary that contains lists of values of different columns.
        """
        primary_keys = [ row[0:4] for row in self.get_data() ]
        if primary_keys:
            tfcintvls, rxratios, tpdusizes, datasizes = zip(*primary_keys)
            return { "tfcintvl": sorted(set(tfcintvls)), 
                     "rxratio":  sorted(set(rxratios)), 
                     "tpdusize": sorted(set(tpdusizes)), 
                     "datasize": sorted(set(datasizes)) }
        else:
            return { "tfcintvl": [], 
                     "rxratio":  [], 
                     "tpdusize": [], 
                     "datasize": [] }


    def load_data_from_file(self, path, mode=0, dryrun=True):
        """Load data from a file specifid by path. It returns True if 
        successful. The name of file must match following regular expression: 
        "(\d+_){4}.+\.log" Conflicted data will be handled depending on the 
        value of mode. Data will be printed if dryrun is True.
        """
        if not os.path.isfile(path):
            raise ValueError("'{}' is not a file.".format(path))
        filename = os.path.basename(path)
            
        if not re.match(r"(\d+_){4}.+\.log", filename):
            raise ValueError("'{}' has a wrong name.".format(filename))
        tfcintvl, rxratio, tpdusize, datasize = filename.split("_")[0:4]
        
        exprdata = []
        with open(path) as f:
            for line in f:
                if "is transmitting to" in line:
                    if line.split(" ... ")[-1].startswith("OK"):
                        # skip lines until "Statistics" shows up
                        line = f.next()
                        while "Statistics" not in line: line = f.next()
                        # parse different metrics
                        metrics = []
                        # load packets, retrans and loss
                        tmp = f.next().split()[1::2]
                        metrics.extend([tmp[0], tmp[1], tmp[2].strip("%")])
                        # load fragmts, frames and bytes
                        tmp = f.next().split()[1::2]
                        metrics.extend([tmp[0], tmp[1], tmp[2]])
                        # load time, dtime
                        tmp = f.next().split()[1:3]
                        metrics.extend([tmp[0], tmp[1].strip("()")])

                        exprdata.append(metrics)
                    else:
                        exprdata.append(["NaN"] * 8)
        if exprdata:
            row = (tfcintvl, rxratio, tpdusize, datasize, np.array(exprdata))
        else:
            row = (tfcintvl, rxratio, tpdusize, datasize, None)
        
        if dryrun:
            print "Data in db (old):", 
            self.print_data(row[0], row[1], row[2], row[3])
            print "Data in file (new):",  
            if row[4] is not None:
                print row[0:4]
                print "\t", str(row[4]).replace("\n", "\n\t")
            else:
                print None
        else:
            self.insert_row(row, mode)


    def load_data_from_directory(self, path, mode=0, pattern=None):
        """Load data from files within a directory specifid by path. The files 
        can also be filtered by pattern which is a regular expression. 
        Conflicted data will be handled depending on the value of mode.
        """
        if not os.path.isdir(path):
            raise ValueError("'{}' is not a directory.".format(path))

        files = [f for f in os.listdir(path)
                    if os.path.isfile(os.path.join(path, f)) and 
                        f.endswith(".log")]
        if pattern:
            files = filter(lambda x: re.search(pattern, x), files)

        for file in files:
            self.load_data_from_file(os.path.join(path, file), mode, False)
            print "{} -> Done.".format(file)

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
    for row in data:
            xdb.insert_row(row)
            # test exception
            xdb.insert_row(row)

    print "*******************"
    for row in xdb.get_data():
        print row

    print "*******************"
    for row in xdb.get_data(-1, 2, -1, 0):
        print row

    print "*******************"
    for row in xdb.get_data(-1, -1, -1, 0):
        print row

    os.remove("test.db")