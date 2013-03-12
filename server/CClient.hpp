/*
 * CClient.hpp
 *
 *  Created on: Mar 29, 2012
 *      Author: Martin Schreiber <martin.schreiber@in.tum.de>
 */

#ifndef CCLIENT_HPP_
#define CCLIENT_HPP_

#include "CResources.hpp"
#include <list>
#include <vector>



/**
 * this class provides a container for all information associated to a client
 */
class CClient
{
	int verbosity_level;

public:
	/**
	 * process id of client
	 */
	pid_t pid;

	/**
	 * unique id of client
	 */
	int client_id;

	/**
	 * constraints / hints: minimum number of cores
	 */
	int constraint_min_cores;

	/**
	 * constraints / hints: maximum number of cores
	 */
	int constraint_max_cores;

	/**
	 * distribution hint
	 */
	float distribution_hint;

	/**
	 * scalability graph
	 */
	std::vector<float> hint_scalability_graph;

	/**
	 * retreat triggered?
	 */
	bool retreat_active;

	/**
	 * nonblocking reinvade information active
	 */
	bool reinvade_nonblocking_active;

	/**
	 * number of assigned cores
	 */
	int number_of_assigned_cores;

	/**
	 * list with assigned cores
	 */
	std::list<int> assigned_cores;



	/**
	 * create new client with corresponding pid
	 */
	CClient(
			pid_t i_pid,			///< process id of client application
			int i_client_id,		///< unique client id
			int i_verbosity_level
	)	:
		verbosity_level(i_verbosity_level),
		pid(i_pid),
		client_id(i_client_id),
		constraint_min_cores(0),
		constraint_max_cores(0),
		distribution_hint(0),
		retreat_active(false),
		reinvade_nonblocking_active(false),
		number_of_assigned_cores(0)
	{
	}



	/**
	 * deconstructor
	 */
	~CClient()
	{
	}



	/**
	 * add a new core which belongs to the client
	 */
	void addCore(
			int i_core_id	///< core id of client application
	)
	{
		assigned_cores.push_back(i_core_id);
		number_of_assigned_cores++;
	}



	/**
	 * remove a core from the list
	 */
	void removeCore(
			int i_core_id
	)
	{
		for (std::list<int>::iterator iter = assigned_cores.begin(); iter != assigned_cores.end(); iter++)
		{
			if (*iter == i_core_id)
			{
				assigned_cores.erase(iter);
				break;
			}
		}

		std::cerr << "ERROR: core " << i_core_id << " not found" << std::endl;
		assert(false);
		exit(-1);
	}



	/**
	 * release all cores associated to the specified client
	 */
	void releaseAllClientCoresAndFreeResources(
			CResources &cResources,
			bool i_skip_first_core = false	///< skip freeing the first core
	)
	{
		if (verbosity_level > 5)
			std::cout << *this << ": releaseAllClientCores" << std::endl;

		std::list<int>::iterator iter = assigned_cores.begin();

		if (i_skip_first_core)
		{
			assert(iter != assigned_cores.end());
			iter++;
		}

		while (iter != assigned_cores.end())
		{
			int core_id = *iter;

			if (verbosity_level > 5)
				std::cout << core_id << ": " << cResources.core_pids[core_id]  << std::endl;

			if (cResources.core_pids[core_id] != pid)
			{
				std::cout << *this << ": CORE ID " << core_id << " not associated with client!" << std::endl;
				assert(false);
				exit(-1);
			}

			cResources.core_pids[core_id] = 0;
			number_of_assigned_cores--;

			if (verbosity_level > 5)
				std::cout << "Releasing core " << core_id << std::endl;

			iter = assigned_cores.erase(iter);
		}

		if (i_skip_first_core)
			assert(number_of_assigned_cores == 1);
		else
			assert(number_of_assigned_cores == 0);
	}


	/**
	 * return the scalability at a specific sampling point (#cpus)
	 */
	float getScalability(
			int i_sampling_point	// sampling point is given in cpu nrs. starting at 1!!!
	)
	{
		if (hint_scalability_graph.size() == 0)
		{
			// return 0 to limit scalability - maybe we should return -1 here to get a really restriction
			if (i_sampling_point > constraint_max_cores)
				return 0;

			// return perfect scalability
			// TODO: return value slightly below the linear scalability
			return i_sampling_point;
		}

		i_sampling_point--;

		if (hint_scalability_graph.size() <= (size_t)i_sampling_point)
			return hint_scalability_graph.back();
		else
			return hint_scalability_graph[i_sampling_point];
	}



	/**
	 * update the scalability graph for a given client
	 */
	void setScalabilityGraph(
			float i_scalability_graph[],
			int i_scalability_graph_size
		)
	{
		assert(i_scalability_graph_size >= 0);

		hint_scalability_graph.resize(i_scalability_graph_size, 0);
		for (int i = 0; i < i_scalability_graph_size; i++)
			hint_scalability_graph[i] = i_scalability_graph[i];
	}



	bool operator==(const CClient &cClient)
	{
		return pid == cClient.pid;
	}



	friend
	::std::ostream&
	operator<<(::std::ostream& os, const CClient &c)
	{
		return os << "Client " << c.client_id << " [" << c.pid << "]";
	}
};


#endif /* CCLIENT_HPP_ */
