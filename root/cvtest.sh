#!/bin/bash

#./runsy3.sh > cv_out.txt 
cat cv_out_expected.txt|grep -f cv_out.txt|  sort | uniq -c
