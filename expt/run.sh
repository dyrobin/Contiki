#! /bin/sh

# handle arguments
if [ $# -ne 2 ] && [ $# -ne 3 ]; then
    echo "Usage: `basename $0` [-d] INTVL(s) RX(%)"
    exit 1
fi

if [ $# -eq 3 ]; then
    if [ $1 != "-d" ]; then
        echo "Usage: `basename $0` [-d] INTVL(s) RX(%)"
        exit 1
    else
        DAEnabled="true"
        shift
    fi
fi

if echo $1 | egrep -q "^([0-9]|[1-9][0-9]|100)$"; then
    TFCINTVL=$1
else
    echo "Invalid INTVL: it must be an integer within [0, 100]"
    exit 1
fi

if echo $2 | egrep -q "^([1-9]|[1-9][0-9]|100)$"; then
    RXPCT=$2
    RXRATIO=`echo ${RXPCT} | awk '{printf("%.2f", $1 / 100)}'`
else
    echo "Invalid RX: it must be an integer within [1, 100]"
    exit 1
fi


# run experiments for different datasize, 
# ${TFCINTVL}, ${RXPCT}, ${TPDUSIZE} must be defined before calling this
run_diff_data() {
    # datasize starts from 1K
    local datasize=512

    for j in `seq 1 5`; do
        datasize=$((${datasize} * 2))
        prefix="${TFCINTVL}_${RXPCT}_${TPDUSIZE}_${datasize}"
        cscfile=${prefix}_${SUFFIX}.csc
        jsfile=${prefix}_${SUFFIX}.js
        logfile=${prefix}_${SUFFIX}.log

        #generate new js file according to the template
        sed -e "s/\(tfc_intvl = \).*/\1${TFCINTVL};/" \
            -e "s/\(rx_pct = \).*/\1${RXPCT};/" \
            -e "s/\(tpdu_size = \).*/\1${TPDUSIZE};/" \
            -e "s/\(data_size = \).*/\1${datasize};/" \
            ${JSTEMPLATE} > ${SIMDIR}/${jsfile}
        
        #generate new csc file according to the template
        sed -e "s/\(<success_ratio_rx>\).*\(<\/success_ratio_rx>\)/\1${RXRATIO}\2/" \
            -e "s/\(<scriptfile>.*\/\)[^\/]*\(<\/scriptfile>\)/\1${jsfile}\2/" \
            ${CSCTEMPLATE} > ${SIMDIR}/${cscfile}

#       # code to support concurrent jobs
#       jobnum=`ps --no-headers -C java | wc -l`
#       while [ ${jobnum} -ge ${CPUNUM} ]; do
#           sleep 10
#           jobnum=`ps --no-headers -C java | wc -l`
#       done

        # Not sure if cooja have concurrent problems or not, so we run the tests
        # one by one
        STIME=`date +%s`
        echo -n "Running ${prefix} ... "
        java -mx512m -jar ${CONTIKIDIR}/tools/cooja/dist/cooja.jar \
             -nogui=${SIMDIR}/${cscfile} -contiki=${CONTIKIDIR} 1>/dev/null

        if [ -f "COOJA.testlog" ] && grep -q "TEST OK" COOJA.testlog; then
            echo -n "OK. "
            mv COOJA.testlog ${LOGDIR}/${logfile}
        else
            echo -n "Failed. "
            if [ -f "COOJA.testlog" ]; then
                mv COOJA.testlog ${LOGDIR}/${logfile}
            fi

            if [ -f "COOJA.log" ]; then
                echo "-- OUTPUT LOG --" >> ${LOGDIR}/${logfile}
                tail -20 COOJA.log >> ${LOGDIR}/${logfile}
            fi
        fi

        rm -f COOJA.log
        ETIME=`date +%s`
        echo "Duration: `date -u -d @$((${ETIME} - ${STIME})) +%H:%M:%S`"
    done
    #wait // concurrent support
}


#    # Max jobs running concurrently
#    CPUNUM=`cat /proc/cpuinfo | grep processor | wc -l`


# create dirs for saving simulation configs and log outputs
SIMDIR=sims
if [ ! -d ${SIMDIR} ]; then
    mkdir ${SIMDIR}
fi

LOGDIR=logs
if [ ! -d ${LOGDIR} ]; then
    mkdir ${LOGDIR}
fi

# several configs
CONTIKIDIR=~/workspace/contiki

SUFFIX=sim15
CSCTEMPLATE=csc_template_${SUFFIX}
JSTEMPLATE=js_template_${SUFFIX}


# do the tests now
STTIME=`date +%s`
if [ ! -z ${DAEnabled} ] && [ ${DAEnabled} = "true" ]; then
    TPDUSIZE=0
    run_diff_data
else
    TPDUSIZE=0
    TPDUSTEP=65
    for i in `seq 1 6`; do
        TPDUSIZE=$((${TPDUSIZE} + ${TPDUSTEP}))
        run_diff_data
    done
fi
ETTIME=`date +%s`
echo "Done. Total Time: `date -u -d @$((${ETTIME} - ${STTIME})) +%H:%M:%S`"
