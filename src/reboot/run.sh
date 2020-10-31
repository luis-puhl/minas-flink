#!/bin/bash

# mkfifo ~/my_fifo
# command1 > ~/my_fifo &
# command2 > ~/my_fifo &
# command3 < ~/my_fifo

# store-service
# cat training.csv | offline | store | eet test.csv | online | tee output.csv | novelty | store

gcc -g -Wall -lm -lpthread src/reboot/main.c -o bin/reboot
cat datasets/training.csv datasets/emtpyline datasets/test.csv | ./bin/reboot > out/reboot.csv 2> experiments/reboot.log
python3 src/evaluation/evaluate.py Mfog-Reboot datasets/test.csv out/reboot.csv experiments/reboot.png >> experiments/reboot.log
