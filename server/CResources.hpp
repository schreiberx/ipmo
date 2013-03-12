/*
 * CResources.hpp
 *
 *  Created on: Mar 01, 2013
 *      Author: Martin Schreiber <martin.schreiber@in.tum.de>
 */

#ifndef CRESOURCES_HPP_
#define CRESOURCES_HPP_

#include <list>
#include <vector>



/**
 * this class provides a container for the resources
 */
class CResources
{
	int verbose_level;

public:
	/**
	 * maximum number of cores installed on system
	 */
	int max_cores;

	/**
	 * pids assigned to one core
	 *
	 * when an 0 value is stored to this vector, this CPU resource is free
	 */
	pid_t *core_pids;


	/**
	 * create new client with corresponding pid
	 */
	CResources(
			int i_max_cores = -1,
			int i_verbose_level = 0
	) :
		verbose_level(-1),
		max_cores(-1)
	{
		// setup maximum number of cores
		if (i_max_cores == -1)
			max_cores = sysconf(_SC_NPROCESSORS_ONLN);
		else
			max_cores = i_max_cores;

		if (verbose_level > 5)
			std::cout << "max cores: " << max_cores << std::endl;

		// pre-allocate
		core_pids = new pid_t[max_cores];

		// setup cores to be associated with no pid
		for (int i = 0; i < max_cores; i++)
			core_pids[i] = 0;
	}


	/**
	 * deconstructor
	 */
	~CResources()
	{
		delete core_pids;
	}


	friend
	::std::ostream&
	operator<<(::std::ostream& os, const CResources &r)
	{
		return os << "Resources" << std::endl;
	}
};


#endif /* CCLIENT_HPP_ */
