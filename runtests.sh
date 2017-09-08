#!/bin/bash

array=($(ls testcases))
fname="results"
resultsdir="testResults/"
myresultsdir="myResults/"
fext=".txt"
difftext="diff"
maxtest=34

mkdir myResults &> /dev/null

count=0

for i in "${array[@]}"
do
   : 

   test="$(cut -d'.' -f1 <<< $i)"
   echo -n "Running $test ....................... "

   make $test &> /dev/null
   eval $test &> $myresultsdir$test$fname$fext
   diffresults="$(diff $resultsdir$test$fext $myresultsdir$test$fname$fext)"
   diffsize=${#diffresults}
   if [ $diffsize -gt 0 ]
   then
   		echo "FAILED"
   else 
   		echo "SUCCEEDED"
   fi

   if [ $count -gt $(expr $maxtest - 1) ]
   then
   		break
   	fi

	count=$(expr $count + 1)

done

make clean &> /dev/null

