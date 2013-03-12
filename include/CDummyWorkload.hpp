/*
 * CDummyWorkload.hpp
 *
 *  Created on: Mar 29, 2012
 *      Author: Martin Schreiber <martin.schreiber@in.tum.de>
 */

#ifndef CDUMMYWORKLOAD_HPP_
#define CDUMMYWORKLOAD_HPP_

#include <cmath>

class CDummyWorkload
{
public:
	static double doSomeSqrt(double i_seed, int iters = 40000)
	{
		double seed = i_seed;

#if 0
	#pragma omp critical
		{
			cpu_set_t s;

			pthread_getaffinity_np(pthread_self(), sizeof(s), &s);
//			std::cout << "AFFINITY MASK for " << pthread_self() << ": " << *(int*)(void*)&s << std::endl;
		}
#endif

		for (int i = 0; i < iters; i++)
			for (int i = 0; i < iters; i++)
				seed = std::sqrt(seed);

		seed += (double)i_seed;
		return seed;
	}
};


#endif /* CDUMMYWORKLOAD_HPP_ */
