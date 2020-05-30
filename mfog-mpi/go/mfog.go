package main

import (
	"fmt"
	"os"

	MPI "github.com/yoo/go-mpi"
)

func main() {
	fmt.Println("Hello, World!")
	err = MPI.Init(os.Args)

	size, err = MPI.Comm_size(MPI.Comm MPI_COMM_WORLD)
	rank, err = MPI.Comm_rank(MPI_COMM_WORLD)
	fmt.Println("MPI %d %d", rank, size)
}
