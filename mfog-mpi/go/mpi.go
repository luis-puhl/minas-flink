// This is a sample MPI program in Go.
//
// To build and run it, we need to install MPI. I downloaded and built
// Open MPI 1.8.8:
//
//    wget https://www.open-mpi.org/software/ompi/v1.8/downloads/openmpi-1.8.8.tar.bz2
//    tar xjf openmpi-1.8.8.tar.bz2
//    cd openmpi-1.8.8
//    ./configure --prefix=/home/yi/openmpi
//    make -j2 install
//
// This installs Open MPI header files and libraries into /home/yi/openmpi/{include,lib}.
//
// Then I install the Go bindings for MPI: https://github.com/JohannWeging/go-mpi
//
//    export CGO_CFLAGS='-I/home/yi/openmpi/include'
//    export CGO_LDFLAGS='-L/home/yi/openmpi/lib -lmpi'
//    go get github.com/JohannWeging/go-mpi
//
// Now we are ready to build this sample program:
//
//    go install
//
// This should generates the binary exectuable `learn-mpi` in $GOPATH/bin.  So we can run it
//
//    LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/yi/openmpi/lib mpirun -n 4 -hostfile /tmp/hosts $GOPATH/bin/learn-mpi
//
// where /tmp/hosts includes only one line:
//
//    localhost
//
// The expected output should be something like:
/*
Hello world from processor centos, rank 3 out of 4 processors
Hello world from processor centos, rank 2 out of 4 processors
Hello world from processor centos, rank 1 out of 4 processors
Hello world from processor centos, rank 0 out of 4 processors
*/
package main

import (
	"fmt"
	"os"

	mpi "github.com/JohannWeging/go-mpi"
)

func main() {
	mpi.Init(&os.Args)
	worldSize, _ := mpi.Comm_size(mpi.COMM_WORLD)
	rank, _ := mpi.Comm_rank(mpi.COMM_WORLD)
	procName, _ := mpi.Get_processor_name()
	fmt.Printf("Hello world from processor %s, rank %d out of %d processors\n", procName, rank, worldSize)
	mpi.Finalize()
}
