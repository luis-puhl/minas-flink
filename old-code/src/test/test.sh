#!/bin/bash

gcc -g -Wall -pthread src/test/circ-buff.c src/reboot/CircularExampleBuffer.c -o bin/cir

./bin/cir 10 10 10 || exit 1
for i in {1..5}; do
    for j in {1..5}; do
        for k in {1..5}; do
            echo "./bin/cir $i $j $k";
            ./bin/cir $i $j $k >/dev/null || exit 1;
        done;
    done;
done;

for i in 1 2 3; do
    /usr/bin/time bash -c "for i in {1..100}; do ./bin/cir $i $i $i >/dev/null || exit 1; done;";
    echo
done;


# 1 mutex
#   5.15s user 15.35s system 619% cpu 3.310 total
#   6.39s user 18.18s system 638% cpu 3.846 total
#   5.65s user 17.04s system 634% cpu 3.574 total
# 2 mutexes (head, tail)
#   1.19s user 3.15s system 208% cpu 2.082 total
#   1.12s user 3.02s system 212% cpu 1.951 total
#   1.12s user 3.03s system 213% cpu 1.947 total
# Signals wont work with 2 mutexes, we lock both, then release one at a time.
#   1.69s user 3.02s system 181% cpu 2.594 total
#   1.56s user 2.92s system 194% cpu 2.303 total
#   1.87s user 3.11s system 177% cpu 2.807 total
# Swapping the lock order does not work.
# Storing index position and then Swapping
#   1.83s user 3.14s system 180% cpu 2.752 total
#   1.71s user 3.20s system 177% cpu 2.772 total
#   1.60s user 3.28s system 178% cpu 2.732 total
#   1.89s user 3.14s system 169% cpu 2.968 total
# Stash and rerun
#   1.75s user 3.12s system 178% cpu 2.725 total
#   1.83s user 3.08s system 176% cpu 2.776 total
#   1.68s user 3.27s system 175% cpu 2.826 total
# Stash pop
#   0.09 user0.04system 0:00.12elapsed 112%CPU (0avgtext+0avgdata 3924maxresident)k
#       0inputs+0outputs (0major+13209minor)pagefaults 0swaps
#   0.12 user0.07system 0:00.15elapsed 134%CPU (0avgtext+0avgdata 3936maxresident)k
#       0inputs+0outputs (0major+13796minor)pagefaults 0swaps
#   0.15 user0.07system 0:00.15elapsed 148%CPU (0avgtext+0avgdata 3824maxresident)k
#       0inputs+0outputs (0major+14453minor)pagefaults 0swaps
