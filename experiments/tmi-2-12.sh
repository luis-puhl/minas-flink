#!/bin/bash

# TIME := /usr/bin/time \
# 	/usr/bin/time --output=experiments/timing.log --append \
# 	--format="%C\n\t%U user\t%S system\t%E elapsed\n\t%P CPU\t(%X avgtext+%D avgdata\t%M maxresident)k\n\t%I inputs+%O outputs\t(%F major+%R minor)pagefaults\t%W swaps\n"

export TIME_FORMAT="%C\n\t%U user\t%S system\t%E elapsed\n\t%P CPU\t(%X avgtext+%D avgdata\t%M maxresident)k\n\t%I inputs+%O outputs\t(%F major+%R minor)pagefaults\t%W swaps\n"
MFOG_NODES=3
cat out/offline-model.csv datasets/test.csv | \
    /usr/bin/time /usr/bin/time --output=experiments/timing.log --append --format="$TIME_FORMAT" \
    mpirun -n $MFOG_NODES -hostfile ./conf/hostsfile ./bin/tmpi 1 1 \
    > out/tmi-rpi-n$MFOG_NODES.csv 2> experiments/tmi-rpi-n$MFOG_NODES.log

