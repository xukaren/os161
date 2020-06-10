./runsp3.sh > output.txt 

#!/bin/bash
egrep -c", max wait 0.0[0-8]" < output.txt > out.txt
egrep ", max wait 0.09*" < output.txt > out.txt
sort -V -o output.sorted output.txt