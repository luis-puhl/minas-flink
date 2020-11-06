#!/bin/bash

# mkfifo ~/my_fifo
# command1 > ~/my_fifo &
# command2 > ~/my_fifo &
# command3 < ~/my_fifo

# store-service
# cat training.csv | offline | store | eet test.csv | online | tee output.csv | novelty | store

gcc -g -Wall -lm -lpthread src/reboot/{main,base}.c -o bin/reboot/serial
cat datasets/training.csv datasets/emtpyline datasets/test.csv \
    | ./bin/reboot/serial > out/reboot-serial.csv 2> experiments/reboot-serial.log
python3 src/evaluation/evaluate.py Mfog-Reboot-serial datasets/test.csv out/reboot-serial.csv \
    experiments/reboot-serial.png >> experiments/reboot-serial.log

gcc -g -Wall -lm -lpthread src/reboot/{offline,base}.c -o bin/reboot/offline
cat datasets/training.csv | ./bin/reboot/offline > out/reboot-offline.csv 2> experiments/reboot.log

gcc -g -Wall -lm -lpthread src/reboot/{online,base}.c -o bin/reboot/online
cat out/reboot-offline.csv datasets/test.csv | ./bin/reboot/online > out/reboot-online.csv 2>> experiments/reboot.log

python3 src/evaluation/evaluate.py Mfog-Reboot datasets/test.csv out/reboot-online.csv \
    experiments/reboot.png >> experiments/reboot.log
