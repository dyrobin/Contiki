#! /bin/sh
#
# Run experiment where variables can be configured by options.
# Authored by Yang Deng <yang.deng@aalto.fi>


print_usage() {
    echo "Usage: $0 [-c contiki_dir] [-i traffic_interval]"
    echo "       [-r success_ratio_rx] [-t tpdu_size]"
    echo "       [-s data_size] output_dir"
}

print_invalid_opt() {
    echo "Invalid \"-${flag}\": \"${OPTARG}\" $1"
}

# default config values
contikidir=`cd .. && pwd`

tfcintvl=0
rxratio=100
tpdusize=65
datasize=1024

# get opts to change config values
while getopts c:i:r:t:s: flag; do
    case ${flag} in
        c)  if [ -d ${OPTARG} ] && 
               echo `cd ${OPTARG} && pwd` | grep -q "contiki$"; then
                contikidir=`cd ${OPTARG} && pwd`
            else
                print_invalid_opt "is not a contiki directory"
                exit 1
            fi
            ;;
        i)  if echo ${OPTARG} | egrep -q "^([0-9]|[1-5][0-9]|60)$"; then
                tfcintvl=${OPTARG}
            else
                print_invalid_opt "is not an integer within [0, 60]"
                exit 1
            fi
            ;;
        r)  if echo ${OPTARG} | egrep -q "^([1-9]|[1-9][0-9]|100)$"; then
                rxratio=${OPTARG}               
            else
                print_invalid_opt "is not an integer within [1, 100]"
                exit 1
            fi
            ;;
        t)  if echo ${OPTARG} | egrep -q "^(0|[1-9][0-9]*)$"; then
                tpdusize=${OPTARG}
            else
                print_invalid_opt "is not an integer"
                exit 1
            fi
            ;;
        s)  if echo ${OPTARG} | egrep -q "^(0|[1-9][0-9]*)$"; then
                datasize=${OPTARG}
            else
                print_invalid_opt "is not an integer"
                exit 1
            fi
            ;;
        ?)  print_usage
            exit 1
            ;;
    esac
done
shift $((${OPTIND} - 1));

if [ $# -ne 1 ]; then
    print_usage
    exit 1
fi

mkdir -p $1
if [ $? -ne 0 ]; then
    exit 1
fi

# main part starts here
prefix="${tfcintvl}_${rxratio}_${tpdusize}_${datasize}"
suffix=sim15

csctemplate=csc_template_${suffix}
jstemplate=js_template_${suffix}
if [ ! -f ${csctemplate} ] || [ ! -f ${jstemplate} ]; then
    echo "Error: ${csctemplate} or ${jstemplate} does not exist."
    exit 1
fi

outputdir=`cd $1 && pwd`
cscfile=${prefix}_${suffix}.csc
jsfile=${prefix}_${suffix}.js
logfile=${prefix}_${suffix}.log

# generate new js file from jstemplate; config values are rewritten here
sed -e "s/\(tfc_intvl = \).*/\1${tfcintvl};/" \
    -e "s/\(rx_ratio = \).*/\1${rxratio};/" \
    -e "s/\(tpdu_size = \).*/\1${tpdusize};/" \
    -e "s/\(data_size = \).*/\1${datasize};/" \
    ${jstemplate} > ${outputdir}/${jsfile}

# generate new csc file from csctemplate; config values are rewritten here
rxratio_f=`echo ${rxratio} | awk '{printf("%.2f", $1 / 100)}'`
sed -e "s/\(<success_ratio_rx>\).*\(<\/success_ratio_rx>\)/\1${rxratio_f}\2/" \
    -e "s/\(<scriptfile>.*\/\)[^\/]*\(<\/scriptfile>\)/\1${jsfile}\2/" \
    ${csctemplate} > ${outputdir}/${cscfile}


# run test
STIME=`date +%s`
echo -n "Running ${prefix} ... "

java -mx1536m -jar ${contikidir}/tools/cooja/dist/cooja.jar \
     -nogui=${outputdir}/${cscfile} -contiki=${contikidir} 1>/dev/null

if [ -f "COOJA.testlog" ] && grep -q "TEST OK" COOJA.testlog; then
    echo -n "OK. "
    mv COOJA.testlog ${outputdir}/${logfile}
else
    echo -n "Failed. "
    if [ -f "COOJA.testlog" ]; then
        mv COOJA.testlog ${outputdir}/${logfile}
    fi

    if [ -f "COOJA.log" ]; then
        echo "-- OUTPUT LOG --" >> ${outputdir}/${logfile}
        tail -20 COOJA.log >> ${outputdir}/${logfile}
    fi
fi
rm -f COOJA.log

ETIME=`date +%s`
echo "Duration: `date -u -d @$((${ETIME} - ${STIME})) +%H:%M:%S`"