/*
 * main.cpp
 *
 *  Created on: Mar 29, 2012
 *      Author: Martin Schreiber <martin.schreiber@in.tum.de>
 */

#define USE_OMP_INSTEAD_OF_TBB		0

#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <errno.h>
#include <pthread.h>
#include <cassert>
#include <signal.h>
#include <vector>

#if USE_OMP
	#include "../include/CPMO_OMP.hpp"
#else
	#include "../include/CPMO_TBB.hpp"
#endif

#include "../server/CWorldScheduler_threaded.hpp"
#include "../include/CDummyWorkload.hpp"
#include <mpi.h>


#if USE_OMP
CPMO_OMP *cPmo = 0;
#else
CPMO_TBB *cPmo = 0;
#endif




int main(int argc, char *argv[])
{
	int max_threads = -1;
	int use_invasic = 1;

	if (argc > 1)
		use_invasic = atoi(argv[1]);

	if (use_invasic)
		std::cout << "INVASIC ACTIVATED" << std::endl;
	else
		std::cout << "INVASIC NOT ACTIVATED" << std::endl;

	int verbose_level = 0;
	if (argc > 2)
		verbose_level = atoi(argv[2]);

	/*
	 * initialize MPI
	 */
	int rank, size;
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	if (size == 1)
	{
		std::cout << "run with `mpirun -n 2 ./build/client_mpi_tbb_release [use invasic (0/1)] [verbose level (-99 for fancy graphics)]`" << std::endl;
		return -1;
	}

	CWorldScheduler_threaded *cWorldScheduler_threaded;

	if (use_invasic > 0)
	{
		/*
		 * allocate & start iPMO server for 1st rank on numa-domain
		 */
		if (rank == 0)
		{
			if (use_invasic == 1)
			{
				cWorldScheduler_threaded = new CWorldScheduler_threaded;

				std::cout << "RANK 0: starting worldscheduler" << std::endl;
				cWorldScheduler_threaded->start(-1, verbose_level, true);
			}
		}

		/*
		 * WAIT FOR WORLD SCHEDULER!
		 */
		MPI_Barrier(MPI_COMM_WORLD);

		/*
		 * initialize iPMO client
		 */
#if USE_OMP
		cPmo = new CPMO_OMP(max_threads);
#else
		cPmo = new CPMO_TBB(max_threads);
#endif

		// initial setup request
		cPmo->invade_blocking(1, 1024, 0, nullptr, (float)1);
	}


	CStopwatch cStopwatch;

	cStopwatch.start();

	/*
	 * start simulation
	 */
	for (int t = 0; t < 100; t++)
	{
		if (rank == 0)
			std::cout << "Timestep: " << t << std::endl;

		// compute some workload
		int workload = (t*((rank+size-1)%size) + (100-t)*rank) % 100;

		// for 4 cores only on my laptop, a linear imbalance is not working on dual-core systems
		workload *= workload;

		std::cout << "    > rank " << rank << " workload: " << workload << std::endl;

		if (use_invasic > 0)
		{
			// is some message about optimizations available?
			cPmo->reinvade_nonblocking();

			// request resource update with new workload
			cPmo->invade_nonblocking(1, 1024, 0, nullptr, (float)workload);
		}


		int n;
		if (use_invasic > 0)
		{
			n = cPmo->getNumberOfThreads();
		}
		else
		{
			n = sysconf(_SC_NPROCESSORS_ONLN)/size;
		}

		/*
		 * spread some bogus workload across n threads
		 */
#if USE_OMP
	#pragma omp parallel for schedule(static, 1)

		for (int i = 0; i < n; i++)
		{
			CDummyWorkload::doSomeSqrt(991290391.0, workload/n);
		}

#else

		tbb::parallel_for(
				0, n, 1,
				[workload,n](int i)
				{
					CDummyWorkload::doSomeSqrt(991290391.0, workload/n);
				}
		);

#endif

		std::cout << "    > rank " << rank << " finished" << std::endl;

		// do some stupid barrier
		MPI_Barrier(MPI_COMM_WORLD);
	}


	double t = cStopwatch.getTimeSinceStart();
	std::cout << "runtime: " << t << std::endl;

	if (use_invasic > 0)
	{
//		NEVER MIX retreat() with nonblocking invade/reinvade!
//	    cPmo_TBB->retreat();

		/*
		 * free iPMO client
		 */
		delete cPmo;

		// wait for all clients to shutdown iPMO
		MPI_Barrier(MPI_COMM_WORLD);

		/*
		 * free iPMO server
		 */
		if (rank == 0)
		{
			if (use_invasic == 1)
				delete cWorldScheduler_threaded;
		}
	}

	MPI_Finalize();

	return EXIT_SUCCESS;
}
