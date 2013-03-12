/*
 * CPMO.hpp
 *
 *  Created on: Mar 28, 2012
 *      Author: Martin Schreiber <martin.schreiber@in.tum.de>
 */

#ifndef CPMO_OMP_HPP_
#define CPMO_OMP_HPP_

#include <omp.h>

#include <pthread.h>
#include <sys/types.h>
#include <sched.h>
#include <vector>
#include <signal.h>
#include <unistd.h>
#include <linux/unistd.h>

#include "CPMO.hpp"



/*
 * invasive client handler from hell
 */
class CPMO_OMP	:
	public CPMO
{
	static pid_t gettid()
	{
		return (pid_t)syscall(__NR_gettid);
	}

	bool delayed_parallel_region_mode;
	int delayed_parallel_region_mode_affinity_cache[1024];

public:
	/**
	 * constructor
	 */
	CPMO_OMP(
			int i_max_threads = -1,
			bool i_delayed_parallel_region_mode = false,
			bool i_wait_for_ack = true	///< specifies for how many mpi nodes to wait
										///< before starting execution in case that MPI is activated
	) :
		CPMO(0, i_wait_for_ack),
		delayed_parallel_region_mode(i_delayed_parallel_region_mode)
	{
		if (i_max_threads <= 0)
			max_threads = omp_get_max_threads();
		else
			max_threads = i_max_threads;

		setNumberOfThreads(1);
	}



	/**
	 * deconstructor
	 */
	virtual ~CPMO_OMP()
	{
	}



	/**
	 * shrink/grow number of threads to use for parallel regions
	 */
	void setNumberOfThreads(int n)
	{
		num_computing_threads = n;


		if (!delayed_parallel_region_mode)
			omp_set_num_threads(n);
	}



	/**
	 * re-update number of threads when out of omp parallel scope
	 */
	void delayedUpdateNumberOfThreads()
	{
		assert(delayed_parallel_region_mode);

		omp_set_num_threads(num_computing_threads);

		// deactivate delayed_parallel_region_mode before calling setAffinities!
		delayed_parallel_region_mode = false;

		setAffinities(delayed_parallel_region_mode_affinity_cache, num_computing_threads);

		delayed_parallel_region_mode = true;
	}



	/**
	 * return the number of running threads
	 */
	int getNumberOfThreads()
	{
		if (num_computing_threads == -1)
		{
			std::cerr << "use setNumberOfThreads before initializing" << std::endl;
			assert(false);
		}

#if DEBUG
		int num_threads;

#pragma omp parallel
#pragma omp single
		{
			num_threads = omp_get_num_threads();
		}

		if (num_threads != num_computing_threads)
		{
			std::cerr << "inconsistent state detected (" << num_threads << " " << num_computing_threads << ")" << std::endl;
			assert(false);
		}
#endif

		return num_computing_threads;
	}


	/**
	 * return the maximum allowed number of running threads
	 */
	int getMaxNumberOfThreads()
	{
#if DEBUG
		std::cout << max_threads << std::endl;
#endif
		return max_threads;
	}



	/**
	 * set affinities for "num_running_threads" threads
	 */
	void setAffinities(
			const int *i_cpu_affinities,
			int i_number_of_cpu_affinities
	)
	{
		if (num_computing_threads == 0)
			return;

		if (delayed_parallel_region_mode)
		{
			for (int i = 0; i < num_computing_threads; i++)
				delayed_parallel_region_mode_affinity_cache[i] = i_cpu_affinities[i];
			return;
		}

		assert(i_number_of_cpu_affinities == num_computing_threads);

		#pragma omp parallel for shared(i_cpu_affinities) schedule(static,1)
		for (int i = 0; i < num_computing_threads; i++)
		{
#if DEBUG
			if (i_cpu_affinities[i] >= max_threads)
			{
#pragma omp critical
				std::cerr << "requested cpu affinity: " << i_cpu_affinities[i] << " with max threads " << max_threads << std::endl;
				assert(false);
			}
			assert(i_cpu_affinities[i] >= 0);
			assert(i_cpu_affinities[i] < max_threads);
#endif
			cpu_set_t cpu_set;
			CPU_ZERO(&cpu_set);
			CPU_SET(i_cpu_affinities[i], &cpu_set);

			int err;
			err = pthread_setaffinity_np(
					pthread_self(),
					sizeof(cpu_set_t),
					&cpu_set
				);

			if (err != 0)
			{
				std::cout << err << std::endl;
				perror("pthread_setaffinity_np");
				assert(false);
				exit(-1);
			}
		}
	}
};


#endif /* CPMO_HPP_ */
