all:
	scons --client=omp -j 4
	scons --client=tbb -j 4
	scons --client=mpi_tbb -j 4


clean:
	rm -rf build/*
