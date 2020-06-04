./runsy2.sh > output.txt 

#!/bin/bash
egrep -c "Starting lock test...\n
cleanitems: Destroying sems, locks, and cvs
\nLock test done." < output.txt
