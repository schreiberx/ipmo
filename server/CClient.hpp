/*
 * CClient.hpp
 *
 *  Created on: Mar 29, 2012
 *      Author: schreibm
 */

#ifndef CCLIENT_HPP_
#define CCLIENT_HPP_

#include <list>
#include <vector>


class CClient
{
public:
	/*
	 * process id of client
	 */
	pid_t pid;

	/*
	 * unique id of client
	 */
	int client_id;

	/*
	 * constraints / hints
	 */
	int constraint_min_cores;	///< minimum number of requested cores
	int constraint_max_cores;	///< maximum number of cores

	float distribution_hint;	///< distribution hint

	std::vector<float> hint_scalability_graph;	///< scalability graph

	bool retreat_active;	///< retreat triggered?

	bool reinvade_nonblocking_active;	///< nonblocking reinvade information active

	/*
	 * assigned cores + list
	 */
	int number_of_assigned_cores;
	std::list<int> assigned_cores;


	/**
	 * create new client with corresponding pid
	 */
	CClient(
			pid_t i_pid,		///< process id of client application
			int i_client_id		///< unique client id
	)	:
		client_id(i_client_id),
		retreat_active(false),
		reinvade_nonblocking_active(false)
	{
		pid = i_pid;
		number_of_assigned_cores = 0;
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
