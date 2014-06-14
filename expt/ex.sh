#! /bin/sh
#
# Run multiple experiments; each experiment is done by run.sh
# Authored by Yang Deng <yang.deng@aalto.fi>


intvl_list="0 5 10"
rx_list="100 85 70 55"
tpdu_list="65"
data_list=



echo ${intvlarr}


for intvl in ${intvlarr}; do
    for rx in 100 85 70 55 ; do
        for tpdu in 1 ; do
            for data in 1; do
#                sh run.sh - - - -
                echo $intvl $rx $tpdu $data
            done
        done
    done
done

