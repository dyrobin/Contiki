#!bin/sh

# failed files 
files="
10_85_260_16384_sim15.log
10_70_0_4096_sim15.log
10_70_0_16384_sim15.log
10_70_65_2048_sim15.log
10_70_65_4096_sim15.log
10_70_130_8192_sim15.log
10_70_130_16384_sim15.log
10_70_260_8192_sim15.log
10_70_260_16384_sim15.log
10_55_0_1024_sim15.log
10_55_0_4096_sim15.log
10_55_0_8192_sim15.log
10_55_260_2048_sim15.log
10_55_260_16384_sim15.log
10_55_325_1024_sim15.log
10_55_325_8192_sim15.log
"

for file in ${files}; do
    i=`echo ${file} | awk -F "_" '{print $1}'`
    r=`echo ${file} | awk -F "_" '{print $2}'`
    t=`echo ${file} | awk -F "_" '{print $3}'`
    s=`echo ${file} | awk -F "_" '{print $4}'`
    sh run.sh -i $i -r $r -t $t -s $s sims_re
done
    
