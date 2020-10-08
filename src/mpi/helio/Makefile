
all: minas1 minas2 minas2.log

clean:
	rm -f minas1 minas2

minas1: minas1.c
	mpicc minas1.c -o minas1 -pthread

minas2: minas2.c
	mpicc minas2.c -o minas2 -pthread

minas2.log: minas2 hostfile
	mpirun -np 3 -hostfile hostfile minas2 | tee $@
