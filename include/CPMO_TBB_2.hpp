/*
 * CPMO_TBB.hpp
 *
 *  Created on: May 21, 2012
 *      Author: Martin Schreiber <martin.schreiber@in.tum.de>
 *
 * Description:
 *
 * Compared to the CPMO_OMP & CPMO_TBB versions, this version does
 * not kill all threads when changing the number of threads.
 *
 * This version starts as many threads as there are cores installed on the system.
 *
 * The threads are stopped from running by enqueueing a task
 * which locks an already locked mutex.
 *
 * The affinities are stored to the task data before the mutex is released.
 * Then the task sets the affinity based on the affinity id.
 */


#ifdef CPMO_TBB_HPP_
#	error "CPMO_TBB not compatible with CPMO_TBB_2"
#endif

#ifndef CPMO_TBB_2_HPP_
#define CPMO_TBB_2_HPP_

#include <tbb/tbb.h>
#include <tbb/task.h>

#include <pthread.h>
#include <sched.h>
#include <vector>
#include <signal.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include "CPMO.hpp"



/**
 * pieces of the following codes are taken from TBB 4.0, src/perf.cpp
 * http://threadingbuildingblocks.org/
 *
 * the code was slightly modified.
 */
class CAffinitySetterTask :
	public tbb::task
{
public:
	/*
	 * core id to pin thread to
	 */
	int core_id;

	tbb::atomic<int> &remaining_threads_for_pinning;


	/**
	 * entrance point for task execution
	 */
	tbb::task* execute ()
	{
		/*
		 * setup CPU mask
		 */
		cpu_set_t orig_mask, target_mask;
		CPU_ZERO(&target_mask);
		CPU_SET(core_id, &target_mask);
		assert(CPU_ISSET(core_id, &target_mask));
		CPU_ZERO(&orig_mask);

		std::cout << "setting affinnity for core id " << core_id << std::endl;

		/*
		 * pin current thread
		 */
		int res = sched_setaffinity(0, sizeof(cpu_set_t), &target_mask);
		assert(res == 0);

		/*
		 * wait until all threads are pinned
		 */
		remaining_threads_for_pinning--;

		std::cout << "waiting for remaining threads" << std::endl;
		while (remaining_threads_for_pinning)
		{
			// wait for remaining threads
			std::cout << "remaining threads for pinning: " << remaining_threads_for_pinning << std::endl;
			__TBB_Yield();
		}
		std::cout << "waiting for remaining threads FIN" << std::endl;

		return nullptr;
	}



public:
	CAffinitySetterTask(
			int i_core_id,
			tbb::atomic<int> &i_remaining_threads_for_pinning
	) :
		core_id(i_core_id),
		remaining_threads_for_pinning(i_remaining_threads_for_pinning)
	{
	}
};


class CGetThreadID :
	public tbb::task
{
public:
	/*
	 * core id to pin thread to
	 */
	int *tid;

	tbb::atomic<int> &remaining_threads_for_pinning;

	/**
	 * entrance point for task execution
	 */
	tbb::task* execute ()
	{
		*tid = (pid_t)syscall(SYS_gettid);

		remaining_threads_for_pinning--;
		while (remaining_threads_for_pinning)
			__TBB_Yield();

		return nullptr;
	}



public:
	CGetThreadID(
			pid_t *i_tid,
			tbb::atomic<int> &i_remaining_threads_for_pinning

	) :
		tid(i_tid),
		remaining_threads_for_pinning(i_remaining_threads_for_pinning)
	{
	}
};




/**
 * Lock task
 */
class CLockTask :
	public tbb::task
{
public:
	// pointer to mutex array
	tbb::mutex *lock_mutex;


	/**
	 * entrance point for task execution
	 */
	tbb::task* execute ()
	{
		lock_mutex->lock();
		lock_mutex->unlock();

		return nullptr;
	}


public:
	CLockTask(
			tbb::mutex *i_lock_mutex
	)	:
		lock_mutex(i_lock_mutex)
	{
	}
};





/*
 * invasive client handler for TBB from hell
 */
class CPMO_TBB_2	:
	public CPMO
{
public:
	/*
	 * tbb task scheduler
	 */
	tbb::task_scheduler_init *tbb_task_scheduler_init;

	tbb::mutex *lock_mutices;

	pid_t *thread_ids;


public:
	/**
	 * constructor
	 */
	CPMO_TBB_2(
			int i_max_threads = -1,		///< maximum number of threads
			bool i_verbosity_level = 0,	///< verbosity level
			bool i_wait_for_ack = true	///< specifies for how many mpi nodes to wait
										///< before starting execution in case that MPI is activated
	)	:
		CPMO(i_verbosity_level, i_wait_for_ack),
		tbb_task_scheduler_init(nullptr)
	{
		if (i_max_threads <= 0)
			max_threads = tbb::task_scheduler_init::default_num_threads();
		else
			max_threads = i_max_threads;

		/*
		 * allocate as many threads as there are cores installed on the system
		 */
		tbb_task_scheduler_init = new tbb::task_scheduler_init(max_threads);


		/*
		 * allocate storage for thread ids
		 */
		thread_ids = new pid_t[max_threads];

		tbb::atomic<int> wait_for_n_threads;
		wait_for_n_threads = max_threads;

		tbb::task_list tl;
		for (int i = 0; i < max_threads; i++)
		{
			// allocate task
			tbb::task &t = *new(tbb::task::allocate_root()) CGetThreadID(&(thread_ids[i]), wait_for_n_threads);

			// set task <-> thread affinity
			t.set_affinity(i+1);

			tl.push_back(t);
		}

		tbb::task::spawn_root_and_wait(tl);


		/*
		 * allocate lock mutices
		 */
		lock_mutices = new tbb::mutex[max_threads]();


		/*
		 * lock mutex and enqueue lock tasks for all but master thread
		 */
//		tbb::task_list  tl;
		for (int i = 1; i < max_threads; i++)
		{
			/*
			 * lock mutex to prohibit work-stealing of thread
			 */
			lock_mutices[i].lock();

			// allocate task
			tbb::task &t = *new(tbb::task::allocate_root()) CLockTask(&(lock_mutices[i]));

			// set task <-> thread affinity
			t.set_affinity(i+1);

			tbb::task::spawn(t);

//			tl.push_back(t);
//			tbb::task::spawn_root_and_wait(t);

			// enqueue not possible since affinities are not supported!
//			tbb::task::enqueue(t, tbb::priority_high);
		}

//		tbb::task::spawn_root_and_wait(tl);

		num_computing_threads = 1;
	}



	/**
	 * deconstructor
	 */
	virtual ~CPMO_TBB_2()
	{
		// unlock all mutices in case that some threads are still locked
		for (int i = num_computing_threads; i < max_threads; i++)
		{
			lock_mutices[i].unlock();
		}

		delete tbb_task_scheduler_init;

		delete thread_ids;

		delete [] lock_mutices;
	}



	/**
	 * shrink/grow number of threads to use for parallel regions
	 */
	void setNumberOfThreads(
			int i_new_num_computing_threads
	)
	{
/*
		std::cout << this_pid << ": setNumberOfThreads" << std::endl;
		std::cout << this_pid << ": num_computing_threads: " << num_computing_threads << std::endl;
		std::cout << this_pid << ": i_new_num_computing_threads: " << i_new_num_computing_threads << std::endl;
*/

		if (num_computing_threads == i_new_num_computing_threads)
		{
			/*
			 * nothing to do
			 */
			return;
		}

		if (num_computing_threads < i_new_num_computing_threads)
		{
			/*
			 * unlock computing threads
			 */

			// unlock all mutices in case that some threads are still locked
			for (int i = num_computing_threads; i < i_new_num_computing_threads; i++)
			{
//				std::cout << this_pid << ": unlocking " << i << std::endl;
				lock_mutices[i].unlock();
			}

			num_computing_threads = i_new_num_computing_threads;
			return;
		}

		if (num_computing_threads > i_new_num_computing_threads)
		{
			/*
			 * lock computing threads
			 */
//			int i_start_thread_num_lock = i_new_num_computing_threads;
			int i_end_thread_num_lock = num_computing_threads;

			// set affinities for ALL tasks due to changing core enumeration!!!
			for (int i = 0; i < i_end_thread_num_lock; i++)
			{
				tbb::task &cLockTask = *new(tbb::task::allocate_root()) CLockTask(&(lock_mutices[i]));
				cLockTask.set_affinity(i+1);
				tbb::task::spawn(cLockTask);
			}

			num_computing_threads = i_new_num_computing_threads;
			return;
		}

		assert(false);
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

		assert(i_number_of_cpu_affinities == num_computing_threads);

//		std::cout << this_pid << ": max_threads: " << max_threads << std::endl;

		for (int i = 0; i < num_computing_threads; i++)
		{
			cpu_set_t orig_mask, target_mask;
			CPU_ZERO(&target_mask);
			CPU_SET(i_cpu_affinities[i], &target_mask);
			assert(CPU_ISSET(i_cpu_affinities[i], &target_mask));
			CPU_ZERO(&orig_mask);

//			std::cout << "setting affinity for thread id " << i << " to core " << i_cpu_affinities[i] << std::endl;
			int res = sched_setaffinity(thread_ids[i], sizeof(cpu_set_t), &target_mask);
			assert(res == 0);
		}
#if 0
		tbb::atomic<int> wait_for_n_threads;
		wait_for_n_threads = num_computing_threads;

		tbb::task_list  tl;
		for (int i = 0; i < num_computing_threads; i++)
		{
//			std::cout << this_pid << ": set affinity for computing thread " << i << " to " << i_cpu_affinities[i] << std::endl;
			tbb::task &t = *new(tbb::task::allocate_root()) CAffinitySetterTask(i_cpu_affinities[i], wait_for_n_threads);
			t.set_affinity(tbb::task::affinity_id(i+1));
			tl.push_back(t);
		}
//		std::cout << this_pid << ": Spawning affinity list" << std::endl;
		tbb::task::spawn_root_and_wait(tl);
//		std::cout << this_pid << ": wait for affinity setters finished" << std::endl;
#endif
	}
};


#endif /* CPMO_TBB_2_HPP_ */
