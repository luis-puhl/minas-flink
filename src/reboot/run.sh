#!/bin/bash

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

gcc -g -Wall -lm -lpthread src/reboot/{detection,base}.c -o bin/reboot/detection
cat out/reboot-offline.csv out/reboot-online.csv | ./bin/reboot/detection > out/reboot-detection.csv 2>> experiments/reboot.log

grep -v -e 'Unknown:' out/reboot-online.csv > out/reboot-matches.csv
grep -e 'Unknown:' out/reboot-online.csv > out/reboot-unknowns.csv
python3 src/evaluation/evaluate.py Mfog-Reboot datasets/test.csv out/reboot-matches.csv \
    experiments/reboot.png >> experiments/reboot.log

# mkfifo ~/my_fifo
# command1 > ~/my_fifo &
# command2 > ~/my_fifo &
# command3 < ~/my_fifo

# offline < training.csv > offline.csv
# mkfifo online-model.csv
# cat offline.csv > online-model.csv
# cat offline.csv test.csv | eet online-model.csv | online | tee >(grep -v -e 'Unknown:' > output.csv) | detection > online-model.csv

gcc -g -Wall -lpthread src/reboot/eet.c -o bin/reboot/eet

./bin/reboot/offline < datasets/training.csv > out/reboot-offline.csv 2> experiments/reboot.log

mkfifo out/reboot-online-model.fifo out/reboot-unknows.fifo

cat out/reboot-offline.csv > out/reboot-online-model.fifo &
./bin/reboot/detection < out/reboot-unknows.fifo > out/reboot-online-model.fifo &
./bin/reboot/eet out/reboot-online-model.fifo datasets/test.csv \
    | tee out/reboot-onlineIput.csv \
    | ./bin/reboot/online \
    | tee >(grep -v -e 'Unknown:' > out/reboot-output.csv) \
    | grep 'Unknown:' | tee out/reboot-unknowns.csv \
    | ./bin/reboot/detection > out/reboot-online-model.fifo

rm out/reboot-online-model.fifo out/reboot-unknows.fifo

# $ mpicc --showme-cc
# gcc -I/usr/local/include -pthread -Wl,-rpath -Wl,/usr/local/lib -Wl,--enable-new-dtags -L/usr/local/lib -lmpi
mpicc -g -Wall -lm src/reboot/{threaded-mpi,base}.c -o bin/reboot/tmpi
