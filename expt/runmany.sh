#! /bin/sh
#
# Run multiple experiments; each experiment is done by run.sh
# Authored by Yang Deng <yang.deng@aalto.fi>

# change following variables to determine which experiments to be done.
intvl_list="0 5 10"
rx_list="100 85 70 55"
tpdu_list=`seq 0 65 390`
data_list="1024 2048 4096 8192 16384"

# run multiple experiments
for intvl in ${intvl_list}; do
    for rx in ${rx_list}; do
        for tpdu in ${tpdu_list} ; do
            for data in ${data_list}; do
                sh run.sh -i ${intvl} -r ${rx} -t ${tpdu} -s ${data} sims_new
            done
        done
    done
done

