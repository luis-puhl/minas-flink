AUTOMAKE_OPTIONS = subdir-objects
bin_PROGRAMS = mfog

mfog_SOURCES = src/main.c src/util/loadenv.c src/mpi/minas-mpi.c src/minas/minas.c
# MPICC
# MPI_CFLAGS
# MPI_CLDFLAGS
mfog_LDFLAGS = $(MPI_CLDFLAGS)

AM_CFLAGS = $(MPI_CFLAGS)
