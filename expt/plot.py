#!/usr/bin/env python

import numpy as np
import matplotlib as mpl
from matplotlib import pyplot as plt
from scipy import stats as st
from scipy import linalg as la
from scipy import optimize as opt
from xprmntsdb import XprmntsDB

class Plot:

    metrics = ["packets", "retrans", "loss(%)", "frags", \
               "frames", "bytes", "time(ms)", "time2(ms)"]
    linemarkers = ["o", "^", "s", "d", "*", "p", "v"]
    colors = ["b", "r", "g", "y", "m", "c", "k"]
    hatches = ["/", "-", "\\", "x", "+", "|", ".", "*", "o", "O"]

    def __init__(self, db):
        if not isinstance(db, XprmntsDB):
            raise ValueError(
                "It requires XprmntsDB but '{}' is passed in.".format(type(db)))
        self.db = db


    def plot1(self, datasize=1024, metric=6):
        dic = self.db.get_colvals()
        if not dic["datasize"] or not dic["rxratio"] or \
           not dic["tfcintvl"] or not dic["tpdusize"]:
            raise ValueError("Data is not complete yet.")

        if datasize not in dic["datasize"]:
            raise ValueError("datasize '{}' does not exist.".format(datasize))

        tfcintvls = dic["tfcintvl"]
        rxratios = dic["rxratio"]

        fig, axes = plt.subplots(len(tfcintvls), len(rxratios), sharex=True)
        fig.suptitle("Fig: {} ({})".format(Plot.metrics[metric], datasize))
    
        for i in range(len(tfcintvls)):
            for j in range(len(rxratios)):
                ax = axes[i][j]
                # for efficiency, use list here instead of ndarray
                boxdata = []
                plotdata = []
                for row in self.db.get_data(tfcintvls[i], rxratios[j], 
                                            -1, datasize):
                    # ingore dynamic adjustment of tpdu size 
#                    if row[2] == 0: continue

                    vlddata = []
                    mean, numsuccs = np.nan, np.nan
                    # compute t and s
                    if row[4] is not None:
                        # remove rows where all items are NaN
                        ogl = row[4].astype(np.float)
                        rmd = ogl[~np.isnan(ogl).all(axis=1)]
                        if rmd.shape[0] == 0:
                            vlddata = []
                            mean, numsuccs = np.inf, 0
                        else:
                            vlddata = rmd[:, metric]
#                            q1 = np.percentile(vlddata, 25)
#                            q3 = np.percentile(vlddata, 75)
#                            meantime = st.tmean(vlddata, (q1, q3))
                            u = vlddata.mean()
                            o = vlddata.std()
                            mean = st.tmean(vlddata, (u-o*2, u+o*2))
                            
                            numtotal, _ = ogl.shape
                            numsuccs, _ = rmd.shape
                            # penalty function for time only
                            if metric == 6 and numsuccs < numtotal:
                                mean = mean * numtotal / numsuccs
                    # save into plotdata
                    plotdata.extend([[row[2], mean, numsuccs]])
                    boxdata.extend([[row[2], vlddata]])
                
                # sort by tpdu size for plotting
                plotdata.sort(key=lambda d: d[0])
                boxdata.sort(key=lambda d: d[0])

                # finalize data for plotting
                plotdata = np.array(plotdata)
                x = plotdata[:, 0]
                y = plotdata[:, 1]
                ytw = plotdata[:, 2]
                box = [ np.array(d[1]) for d in boxdata ]

                # plot
                ax.plot(x, y, "-o")
                ax.boxplot(box, positions=x, widths=40)
                
                # twin plot
                axtw = ax.twinx()
                axtw.plot(x, ytw, "r")
                
                # set attributes
                ax.set_title("tfcintvl={} rxratio={}".format(tfcintvls[i], 
                                                              rxratios[j]))
                ax.set_xticks(x)
                ax.set_xlim(x.min()-30, x.max()+30)
                axtw.set_ylim(-0.5, ytw.max()+0.5)

        plt.show()


    def plot2(self, datasize=1024):
        dic = self.db.get_colvals()
        if not dic["datasize"] or not dic["rxratio"] or \
           not dic["tfcintvl"] or not dic["tpdusize"]:
            raise ValueError("Data is not complete yet.")

        if datasize not in dic["datasize"]:
            raise ValueError("datasize '{}' does not exist.".format(datasize))

        tfcintvls = dic["tfcintvl"]
        rxratios = dic["rxratio"]
        
        fig, axes = plt.subplots(len(tfcintvls), len(rxratios))
        fig.suptitle("Fig: coefs of linear ({})".format(datasize))
        fig.subplots_adjust(wspace=0.3)

        sub = [0.4, 0.65, 0.55, 0.3]

        for i in range(len(tfcintvls)):
            for j in range(len(rxratios)):
                ax = axes[i][j]
                ax.set_ymargin(0.05)
                # new axsub by determining its rect
                bb = ax.get_position(True)
                xpos = bb.xmin + bb.width * sub[0]
                ypos = bb.ymin + bb.height * sub[1]
                width = bb.width * sub[2]
                height = bb.height * sub[3]
                axsub = fig.add_axes([xpos, ypos, width, height])

                plotdata = []
                for row in self.db.get_data(tfcintvls[i], rxratios[j], 
                                            -1, datasize):
                    # ingore dynamic adjustment of tpdu size 
                    if row[2] == 0: continue

                    a, b, e = np.nan, np.nan, np.nan
                    if row[4] is not None:
                        # remove rows where all items are NaN
                        ogl = row[4].astype(np.float)
                        rmd = ogl[~np.isnan(ogl).all(axis=1)]
                        # at least 2 samples are needed for fitting
                        if rmd.shape[0] >= 2:
                            x = rmd[:, 1]
                            y = rmd[:, 6]
                            axsub.scatter(x, y, s=20, c=Plot.colors[row[2]/65],  
                                        marker=Plot.linemarkers[row[2]/65])

                            # fitting curve: y=ax+b
                            A = np.vstack((x, np.ones(x.size))).T
                            fit = la.lstsq(A, y)
                            # check if fitting succeeds
                            if fit[0].ndim == 1 and fit[0].size == 2:
                                a, b = fit[0]
                                if abs(a) < 1.0e-6: a = 3000
                                if fit[1]:
                                    e = np.sqrt(fit[1] / x.size) / \
                                            (y.max() - y.min())
                                else:
                                    e = 0
                    plotdata.extend([[row[2], a, b, e]])
                
                # sort by tpdu size for plotting
                plotdata.sort(key=lambda d: d[0])

                # finalize data for plotting
                plotdata = np.array(plotdata)
                x = plotdata[:, 0]
                y = plotdata[:, 2]
                ytw = plotdata[:, 1]
                labels = ["{:.2%}".format(e) for e in plotdata[:, 3]]

                # plot
                ax.plot(x, y, "-bo")
                for k in range(x.size):
                    ax.annotate(labels[k], xy=(x[k], y[k]), xytext=(-15, -15),
                                textcoords="offset points", size="small")

                # twin plot
                axtw = ax.twinx()
                axtw.plot(x, ytw, "-rs")

                # set attributes
                ax.set_title("tfcintvl={} rxratio={}".format(tfcintvls[i], 
                                                              rxratios[j]))
                ax.set_xticks(x)
                ax.set_xlim(x.min()-30, x.max()+30)
                axtw.set_ylim(2000, 4000)
                axsub.xaxis.set_major_locator(
                        mpl.ticker.MaxNLocator(integer=True))
                axsub.tick_params(labelsize="x-small")

        plt.show()


    def plot3(self, datasize=1024, metricx=1, metricy=6):
        dic = self.db.get_colvals()
        if not dic["datasize"] or not dic["rxratio"] or \
           not dic["tfcintvl"] or not dic["tpdusize"]:
            raise ValueError("Data is not complete yet.")

        if datasize not in dic["datasize"]:
            raise ValueError("datasize '{}' does not exist.".format(datasize))

        tfcintvls = dic["tfcintvl"]
        rxratios = dic["rxratio"]
        
        fig, axes = plt.subplots(len(tfcintvls), len(rxratios))
        fig.suptitle("Fig: {} VS {} ({})".format(Plot.metrics[metricx], 
                                            Plot.metrics[metricy], datasize))

        for i in range(len(tfcintvls)):
            for j in range(len(rxratios)):
                ax = axes[i][j]
                for row in self.db.get_data(tfcintvls[i], rxratios[j], 
                                            -1, datasize):
                    # ingore dynamic adjustment of tpdu size 
                    if row[2] == 0: continue
                    # compute k and b
                    if row[4] is not None:
                        # remove rows where all items are NaN
                        ogl = row[4].astype(np.float)
                        rmd = ogl[~np.isnan(ogl).all(axis=1)]
                        # at least 2 samples are needed for fitting
                        if rmd.shape[0] >= 2:
                            x = rmd[:, metricx]
                            y = rmd[:, metricy]
                            ax.scatter(x, y, s=40, c=Plot.colors[row[2]/65],  
                                        marker=Plot.linemarkers[row[2]/65])

                # set attributes
                ax.set_title("tfcintvl={} rxratio={}".format(tfcintvls[i], 
                                                              rxratios[j]))
                ax.xaxis.set_major_locator(
                        mpl.ticker.MaxNLocator(integer=True))

        plt.show()


    def plot33(self, tfcintvl, rxratio, datasize):
        ax = plt.gca()
        for row in self.db.get_data(tfcintvl, rxratio, -1, datasize):
            # ingore dynamic adjustment of tpdu size 
            if row[2] == 0: continue

            if row[4] is not None:
                # remove rows where all items are NaN
                ogl = row[4].astype(np.float)
                rmd = ogl[~np.isnan(ogl).all(axis=1)]
                # at least 2 samples are needed for fitting
                if rmd.shape[0] >= 2:
                    x = rmd[:, 1]
                    y = rmd[:, 6]
                    ax.scatter(x, y, s=40, c=Plot.colors[row[2]/65],  
                                marker=Plot.linemarkers[row[2]/65],
                                label="size {}".format(row[2]/65))

        # set attributes
#        ax.set_title("DATA={}K, FER={}%, INTERVAL={}s".format(
#                    datasize/1024, 100-rxratio, tfcintvl))
        ax.legend(loc=4, fontsize="x-large")
        
        ax.xaxis.set_major_locator(mpl.ticker.MaxNLocator(integer=True))
        ax.tick_params(labelsize="xx-large")
        ax.set_xlabel("Number of Retransmissions", fontsize="xx-large")
        ax.set_ylabel("End-to-End Transmission Time [ms]", fontsize="x-large")

        plt.tight_layout()
        plt.show()


    def plot22(self, tfcintvl, rxratio, datasize):
        plotdata = []
        for row in self.db.get_data(tfcintvl, rxratio, -1, datasize):
            # ingore dynamic adjustment of tpdu size 
            if row[2] == 0: continue

            a, b, e = np.nan, np.nan, np.nan
            if row[4] is not None:
                # remove rows where all items are NaN
                ogl = row[4].astype(np.float)
                rmd = ogl[~np.isnan(ogl).all(axis=1)]
                # at least 2 samples are needed for fitting
                if rmd.shape[0] >= 2:
                    x = rmd[:, 1]
                    y = rmd[:, 6]
                    # fitting curve: y=ax+b
                    A = np.vstack((x, np.ones(x.size))).T
                    fit = la.lstsq(A, y)
                    # check if fitting succeeds
                    if fit[0].ndim == 1 and fit[0].size == 2:
                        a, b = fit[0]
                        if abs(a) < 1.0e-6: a = 3000
                        if fit[1]:
                            e = np.sqrt(fit[1] / x.size) / \
                                    (y.max() - y.min())
                        else:
                            e = 0
            plotdata.extend([[row[2], a, b, e]])

        # sort by tpdu size for plotting
        plotdata.sort(key=lambda d: d[0])

        # finalize data for plotting
        plotdata = np.array(plotdata)
        x = plotdata[:, 0]
        x = np.arange(1, x.size + 1)
        y = plotdata[:, 2]
        ytw = plotdata[:, 1]       
        nrmsd = ["{:.2%}".format(e) for e in plotdata[:, 3]]


        # curve fit (negative exponential)
        def func(x, a, b, c):
            return a * np.exp(-b * x) + c
        popt, pcov = opt.curve_fit(func, x, y, p0=[1000, 1, 1])
        eq = r"${:.2f}e^{{-{:.2f}x}}+{:.2f}$".format(popt[0], popt[1], popt[2])
        
        # plot
        ax = plt.gca()
        ln1 = ax.plot(x, popt[0] * np.exp(-popt[1] * x) + popt[2], "--", label="Fitting Curve")
        ln2 = ax.plot(x, y, "bo", label="Intercept b")
        for k in range(x.size):
            ax.annotate(nrmsd[k], xy=(x[k], y[k]), xytext=(-10, 5),
                        textcoords="offset points", size="x-large")
        ax.annotate(eq, xy=(1.5, 31000), xytext=(0, 0), 
                        textcoords="offset points", size="xx-large")

        # twin plot
        axtw = ax.twinx()
        ln3 = axtw.plot(x, ytw, "-rs", label="Slope m")

        # set attributes
#        lax.set_title("DATA={}K, FER={}%, INTERVAL={}s".format(
#                    datasize/1024, 100-rxratio, tfcintvl))

        lns = ln1 + ln2 + ln3
        labs = [l.get_label() for l in lns]
        ax.legend(lns, labs, loc=1)


        ax.set_xticks(x)
        ax.set_xlim(x.min()-0.5, x.max()+0.5)
        axtw.set_ylim(2000, 4000)


        ax.tick_params(labelsize="xx-large")
        ax.set_xlabel("Size of Packet (Number of 6lo Fragments)", fontsize="xx-large")
        ax.set_ylabel("Ideal End-to-End Transmission Time [ms] \n(Intercept b)", fontsize="x-large")

        axtw.tick_params(labelsize="xx-large")
        axtw.set_ylabel("Average Retransmission Timer [ms] \n(Slope m)", fontsize="x-large")

        plt.tight_layout()
        plt.show()


    def plot11(self, tfcintvl, rxratio, datasize, metric=6):
        ax = plt.gca()

        boxdata = []
        plotdata = []
        for row in self.db.get_data(tfcintvl, rxratio, -1, datasize):
            vlddata = []
            mean, numsuccs = np.nan, np.nan
            # compute t and s
            if row[4] is not None:
                # ingore dynamic adjustment of tpdu size 
                if row[2] == 0: continue

                # remove rows where all items are NaN
                ogl = row[4].astype(np.float)
                rmd = ogl[~np.isnan(ogl).all(axis=1)]
                if rmd.shape[0] == 0:
                    vlddata = []
                    mean, numsuccs = np.inf, 0
                else:
                    vlddata = rmd[:, metric]
#                           q1 = np.percentile(vlddata, 25)
#                           q3 = np.percentile(vlddata, 75)
#                           meantime = st.tmean(vlddata, (q1, q3))
                    u = vlddata.mean()
                    o = vlddata.std()
                    mean = st.tmean(vlddata, (u-o*2, u+o*2))
                    
                    numtotal, _ = ogl.shape
                    numsuccs, _ = rmd.shape
                    # penalty function for time only
#                        if metric == 6 and numsuccs < numtotal:
#                            mean = mean * numtotal / numsuccs
            # save into plotdata
            plotdata.extend([[row[2], mean, numsuccs]])
            boxdata.extend([[row[2], vlddata]])
    
        # sort by tpdu size for plotting
        plotdata.sort(key=lambda d: d[0])
        boxdata.sort(key=lambda d: d[0])

        # finalize data for plotting
        plotdata = np.array(plotdata)
        x = plotdata[:, 0]
        x = np.arange(1, x.size + 1)
        y = plotdata[:, 1]
        ytw = plotdata[:, 2]
        box = [ np.array(d[1]) for d in boxdata ]

        # plot
        ax.plot(x, y, "-o", label="mean value")
        ax.boxplot(box, positions=x, widths=0.5)

#        ax.set_title("DATA={}K, FER={}%, INTERVAL={}s".format(
#                datasize/1024, 100-rxratio, tfcintvl))

#        if metric == 6:
#            ax.set_ylabel("End-to-End Transmission Time [ms]")
#        elif metric == 5:
#            ax.set_ylabel("Total Transmitted Octets")

        ax.legend(fontsize="xx-large")
        ax.set_xlabel("Size of Packet (Number of 6lo Fragments)", fontsize="xx-large")
        
        plt.xticks(fontsize="xx-large")
        plt.yticks(fontsize="xx-large")

        plt.tight_layout()
        plt.show()


    def plot44(self, tfcintvl, datasize, metric=6, showsuccs=False):
        dic = self.db.get_colvals()
        if not dic["datasize"] or not dic["rxratio"] or \
           not dic["tfcintvl"] or not dic["tpdusize"]:
            raise ValueError("Data is not complete yet.")

        if datasize not in dic["datasize"]:
            raise ValueError("datasize '{}' does not exist.".format(datasize))


        bardata = []
        # generate dynamic adjustment of tpdu size
        for tpdusize in [0, 65, 260]:
            itemdata = []
            for row in self.db.get_data(tfcintvl, -1, tpdusize, datasize):
                if row[4] is not None:
                    # remove rows where all items are NaN
                    ogl = row[4].astype(np.float)
                    rmd = ogl[~np.isnan(ogl).all(axis=1)]
                    if rmd.shape[0] == 0:
                        vlddata = []
                        mean, std = np.inf, np.inf
                    else:
                        vlddata = rmd[:, metric]
    #                           q1 = np.percentile(vlddata, 25)
    #                           q3 = np.percentile(vlddata, 75)
    #                           meantime = st.tmean(vlddata, (q1, q3))
                        u = vlddata.mean()
                        o = vlddata.std()
                        mean = st.tmean(vlddata, (u-o*2, u+o*2))
                        std = st.tstd(vlddata, (u-o*2, u+o*2))
                            
                        numtotal, _ = ogl.shape
                        numsuccs, _ = rmd.shape
                        # penalty function for time only
                        if numsuccs < numtotal:
                            mean = mean * numtotal / numsuccs
                        
                # save into plotdata
                itemdata.extend([[row[1], mean, std, numsuccs * 100 / numtotal]])
            
            itemdata.sort(key=lambda d: -d[0])
            bardata.extend([[tpdusize, itemdata]])


        rxratios = dic["rxratio"]
        fers = [100-x for x in rxratios]
        fers.sort()
        index = np.arange(len(fers))
        width = 1.0 / (len(bardata) + 1)


        plt.margins(0.05, 0)

        for i in range(len(bardata)):
            label = bardata[i][0] / 65

            if label == 0:
                label = "Adaptive"
            elif label == 1:
                label = "Contiki"
            else:
                label = "Fixed-size({})".format(label)

            itemdata = np.array(bardata[i][1])

            if showsuccs:
                plt.bar(index + width * i, itemdata[:, 3], width, 
                    fill=True,
                    alpha=0.4,
                    color=Plot.colors[i],
                    hatch=Plot.hatches[i],
                    ecolor='black', label=label)
                plt.legend(loc=1, fontsize="x-large")
#                plt.ylabel("Percent of Successful Transmissions")
            else:
                plt.bar(index + width * i, itemdata[:, 1], width, 
                    yerr=itemdata[:, 2], 
                    fill=True,
                    alpha=0.4,
                    color=Plot.colors[i],
                    hatch=Plot.hatches[i],
                    ecolor='black', label=label)
                plt.legend(loc=2, fontsize="x-large")
#                if metric == 6:
#                    plt.ylabel("Estimated End-to-End Transmission Time [ms]")
#                elif metric == 5:
#                    plt.ylabel("Estimated Total Transmitted Octets")                 


#        plt.title("DATA={}K, INTERVAL={}s".format(datasize/1024, tfcintvl))
        plt.xticks(index+width*len(bardata)/2, fers, fontsize="xx-large")
        plt.yticks(fontsize="xx-large")
        plt.xlabel("FER", fontsize="xx-large")

        plt.axis('tight')

        plt.tight_layout()
        plt.show()




