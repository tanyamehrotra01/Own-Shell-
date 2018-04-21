#!/bin/bash

FILE1=file1.txt
FILE2=file2.txt

if [ $1 == 1 ] 
	then cat "$FILE1"
else 
	cat "$FILE2"
fi

