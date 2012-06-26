/*
 * CWorldScheduler_threaded.hpp
 *
 *  Created on: Jun 18, 2012
 *      Author: schreibm
 */

#ifndef CWORLDSCHEDULER_THREADED_HPP_
#define CWORLDSCHEDULER_THREADED_HPP_

#include "CWorldScheduler.hpp"
#include <pthread.h>

class CWorldScheduler_threaded
{
	pthread_t thread;

	CWorldScheduler *cWorldScheduler;

public:
	CWorldScheduler_threaded()	:
		cWorldScheduler(nullptr)
	{
	}


	static void* worldSchedulerThread(void* ptr)
	{
		CWorldScheduler_threaded *w = (CWorldScheduler_threaded*) ptr;

		while (true)
		{
			if (!w->cWorldScheduler->action())
				break;
		}

		return 0;
	}

	void start(
			int i_max_cores = -1,		///< initialize system with max-cores
			int i_verbose_level = 2,	///< verbosity level
			bool i_color_mode = false	///< use colored output
	)
	{
		assert(cWorldScheduler == nullptr);

		cWorldScheduler = new CWorldScheduler(i_max_cores, i_verbose_level, i_color_mode);

		pthread_create(&thread, NULL, &worldSchedulerThread, this);
	}


	void shutdown()
	{
		if (cWorldScheduler != nullptr)
		{
			// send shutdown message
			cWorldScheduler->selfShutdown();

			// wait for quit
			pthread_join(thread, NULL);

			// cleanup
			delete cWorldScheduler;
			cWorldScheduler = nullptr;
		}
	}

	~CWorldScheduler_threaded()
	{
		shutdown();
	}
};


#endif /* CWORLDSCHEDULER_THREADED_HPP_ */
