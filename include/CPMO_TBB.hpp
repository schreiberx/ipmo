/*
 * CPMO_TBB.hpp
 *
 *  Created on: May 21, 2012
 *      Author: schreibm
 */

#ifndef CPMO_TBB_HPP_
#define CPMO_TBB_HPP_

#include <tbb/tbb.h>
#include <tbb/task.h>

#include <pthread.h>
#include <sched.h>
#include <vector>
#include <signal.h>
#include <unistd.h>
#include <linux/unistd.h>

#include "CPMO.hpp"


/*
 * pieces of the following codes are from TBB 4.0, src/perf.cpp
 * http://threadingbuildingblocks.org/
 *
 * the code was slightly modified.
 */


pid_t gettid()
{
	return (pid_t)syscall(__NR_gettid);
}


bool PinTheThread(
		int cpu_idx,
		tbb::atomic<int>& nRemainingThreadsForPinning
)
{
	cpu_set_t orig_mask, target_mask;
	CPU_ZERO(&target_mask);
	CPU_SET(cpu_idx, &target_mask);
	assert(CPU_ISSET(cpu_idx, &target_mask));
	CPU_ZERO(&orig_mask);

	int res = sched_getaffinity(gettid(), sizeof(cpu_set_t), &orig_mask);
	assert(res == 0);

	res = sched_setaffinity(gettid(), sizeof(cpu_set_t), &target_mask);
	assert(res == 0);

	--nRemainingThreadsForPinning;
	while (nRemainingThreadsForPinning)
		__TBB_Yield();

	return true;
}



class CAffinitySetterTask :
	public tbb::task
{
public:
	static bool m_result;
	static tbb::atomic<int> m_nThreads;
	int m_idx;

	tbb::task* execute ()
	{
		m_result = PinTheThread(m_idx, m_nThreads);
		return NULL;
	}

public:
	CAffinitySetterTask(int idx) :
		m_idx(idx)
	{}

	friend bool AffinitizeTBB ( int, int /*mode*/ );
};


bool CAffinitySetterTask::m_result;
tbb::atomic<int> CAffinitySetterTask::m_nThreads;




/*
 * invasive client handler for TBB from hell
 */
class CPMO_TBB	:
	public CPMO
{
public:
	tbb::task_scheduler_init *tbb_task_scheduler_init;


public:
	/**
	 * constructor
	 */
	CPMO_TBB(int i_max_threads = -1)	:
		CPMO(i_max_threads)
	{
		if (max_threads <= 0)
			max_threads = tbb::task_scheduler_init::default_num_threads();
		else
			max_threads = i_max_threads;

		tbb_task_scheduler_init = new tbb::task_scheduler_init(1);
		num_running_threads = 1;
	}



	/**
	 * deconstructor
	 */
	virtual ~CPMO_TBB()
	{
		delete tbb_task_scheduler_init;
	}



	/**
	 * shrink/grow number of threads to use for parallel regions
	 */
	void setNumberOfThreads(int n)
	{
		num_running_threads = n;

		delete tbb_task_scheduler_init;

		tbb_task_scheduler_init = new tbb::task_scheduler_init(num_running_threads);
	}


	void delayedUpdateNumberOfThreads()
	{
		// not available / meaningful for tbb
		assert(false);
	}


	/**
	 * return the number of running threads
	 */
	int getNumberOfThreads()
	{
		return num_running_threads;
	}



	/**
	 * return the maximum allowed number of running threads
	 */
	int getMaxNumberOfThreads()
	{
#if DEBUG
		std::cout << tbb::task_scheduler_init::default_num_threads() << std::endl;
#endif
		return tbb::task_scheduler_init::default_num_threads();
	}



	/**
	 * set affinities for "num_running_threads" threads
	 */
	void setAffinities(
			const int *i_cpu_affinities,
			int i_number_of_cpu_affinities
	)
	{
		if (num_running_threads == 0)
			return;

		assert(i_number_of_cpu_affinities == (size_t)num_running_threads);

		CAffinitySetterTask::m_result = true;
		CAffinitySetterTask::m_nThreads = i_number_of_cpu_affinities;

		tbb::task_list  tl;
		for (int i = 0; i < i_number_of_cpu_affinities; i++)
		{
			tbb::task &t = *new(tbb::task::allocate_root()) CAffinitySetterTask(i_cpu_affinities[i]);
			t.set_affinity(tbb::task::affinity_id(i+1));
			tl.push_back(t);
		}
		tbb::task::spawn_root_and_wait(tl);


	}
};


#endif /* CPMO_HPP_ */
