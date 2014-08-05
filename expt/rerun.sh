#!/bin/sh

# failed files 
files=""

for file in ${files}; do
    i=`echo ${file} | awk -F "_" '{print $1}'`
    r=`echo ${file} | awk -F "_" '{print $2}'`
    t=`echo ${file} | awk -F "_" '{print $3}'`
    s=`echo ${file} | awk -F "_" '{print $4}'`
    sh run.sh -i $i -r $r -t $t -s $s sims_re
done
    
