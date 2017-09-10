#!/bin/bash

array=($(ls testcases))
fname="results"
resultsdir="testResults/"
myresultsdir="myResults/"
fext=".txt"
difftext="diff"
maxtest=36
diffdir="diffOutputs/"

rm myResults/*
rm diffOutputs/*
mkdir myResults &> /dev/null
mkdir diffOutputs &> /dev/null


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

   if [ $test == "test03" ] || [ $test == "test04" ] || [ $test == "test10" ] || [ $test == "test13" ] || [ $test == "test14" ] || [ $test == "test15" ] || [ $test == "test16" ] || [ $test == "test23" ] || [ $test == "test24" ]; then
         echo "CHECK DIFF"
         diff $resultsdir$test$fext $myresultsdir$test$fname$fext &> $diffdir$test$difftext$fext
   elif [ $diffsize -gt 0 ]; then
   		echo "FAILED"
         diff $resultsdir$test$fext $myresultsdir$test$fname$fext &> $diffdir$test$difftext$fext
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

