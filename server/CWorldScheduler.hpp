/*
 * CWorldScheduler.hpp
 *
 *  Created on: Mar 28, 2012
 *      Author: Martin Schreiber <martin.schreiber@in.tum.de>
 */

#ifndef CWORLDSCHEDULER_HPP_
#define CWORLDSCHEDULER_HPP_

#include <string.h>
#include <stdlib.h>
#include <list>
#include <algorithm>
#include <omp.h>
#include <cmath>
#include <signal.h>
#include <unistd.h>

#include "../include/CMessageQueueServer.hpp"
#include "../include/CMessageQueueClient.hpp"
#include "../include/SPMOMessage.hpp"
#include "../include/CStopwatch.hpp"

#include "CCommonData.hpp"
#include "CClient.hpp"
#include "CResources.hpp"

#include "CMessages_Outgoing.hpp"



/**
 * world scheduler managing the clients
 */
class CWorldScheduler
{
	/**
	 * common data
	 */
	CCommonData cCommonData;

	/**
	 * message queue for communication
	 */
	CMessageQueueServer *cMessageQueueServer;


	/**
	 * Resources
	 */
	CResources cResources;


	/**
	 * client id enumeration
	 */
	int client_enumerator_id;


	/**
	 * list of clients
	 */
	std::list<CClient> clients;


	/**
	 * delayed setup ACKs
	 */
	std::list<CClient*> delayed_setup_acks_clients;


	/**
	 * vector storing the optimal cpu distribution
	 *
	 * each entry is associated with a client as they are stored in the client list
	 */
	std::vector<int> optimal_cpu_distribution;

	/**
	 * timer
	 */
	CStopwatch cStopwatch;


	/**
	 * outgoing messages
	 */
	CMessages_Outgoing cMessages_Outgoing;



public:
	/**
	 * setup world scheduler with given number of cores
	 */
	CWorldScheduler(
			int i_max_cores = -1,		///< initialize system with max-cores
			int i_verbose_level = 2,	///< verbosity level
			bool i_color_mode = false	///< use colored output
	)	:
		cCommonData(i_verbose_level, i_color_mode),
		cResources(i_max_cores, i_verbose_level),
		client_enumerator_id(1)
	{
		cStopwatch.start();

		cMessageQueueServer = new CMessageQueueServer(cCommonData.verbosity_level);

		cMessages_Outgoing.setup(cMessageQueueServer, &cCommonData);
	}



	/**
	 * output client shutdown hints
	 */
	void printClientsShutdownHint()
	{
		if (cCommonData.verbosity_level >= 2)
		{
			std::cout << "sum_client_shutdown_hint: " << cCommonData.sum_client_shutdown_hint << std::endl;
			std::cout << "sum_client_shutdown_hint_div_time: " << cCommonData.sum_client_shutdown_hint_div_time << std::endl;
		}
	}



	/**
	 * deconstructor
	 *
	 * print shutdown hints
	 */
	~CWorldScheduler()
	{
		printClientsShutdownHint();

		delete cMessageQueueServer;

		signal(SIGABRT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
		signal(SIGINT, SIG_DFL);
	}




	/**
	 * search for client information and return array index via o_clientVecId
	 *
	 * \return pointer to the client information
	 */
	CClient* searchClient(
			pid_t i_pid,		///< process id
			int *o_clientVecId = 0	///< output: index client id
	)
	{
		int c = 0;
		for (std::list<CClient>::iterator i = clients.begin(); i != clients.end(); i++)
		{
			if ((*i).pid == i_pid)
			{
				if (o_clientVecId != 0)
					*o_clientVecId = c;
				return &(*i);
			}
			c++;
		}

		if (cCommonData.verbosity_level > 3)
			std::cout << "CLIENT NOT FOUND! (" << i_pid << ") > ignoring" << std::endl;

		return 0;
	}




	/**
	 * send asynchronous reinvade answers
	 */
	void sendAsyncReinvadeAnswers()
	{
		/**
		 * send asynchronous invade information
		 */
		int a = 0;
		for (std::list<CClient>::iterator c = clients.begin(); c != clients.end(); c++)
		{
			applyNewOptimumForClientAsync(&*c, a, false);
			a++;
		}
	}




	/**
	 * search for ACKs which have to be send due to an unsuccessful invade
	 */
	void searchAndSendDelayedACKs()
	{
		int a = -1;
		for (std::list<CClient*>::iterator i = delayed_setup_acks_clients.begin(); i != delayed_setup_acks_clients.end(); i++)
		{
			a++;

			CClient *cClient = *i;

			// search for client id
			int clientVecId;
			CClient *c = searchClient(cClient->pid, &clientVecId);

			if (c == 0)
			{
				// client was not found and deleted => simply remove from delayed ack client list
				i = delayed_setup_acks_clients.erase(i);
				continue;
			}

			// search whether there's a new optimum
			bool anythingChanged = applyNewOptimumForClient(*c, clientVecId);

			if (!anythingChanged)
				continue;

			assert(cClient->number_of_assigned_cores != 0);

			i = delayed_setup_acks_clients.erase(i);

			if (cCommonData.verbosity_level > 2)
				std::cout << "SENDING DELAYED INVADE ACK (" << cClient->pid << ")" << std::endl;

			validateResources();

			updateResourceDistributionAndSendClientMessage(cClient, true);

			applyNewOptimumForClientAsync(cClient, a, false);

			printCurrentState("delayed ack", cClient->client_id);
		}
	}



	void updateResourceDistributionAndSendClientMessage(
			CClient *i_cClient,			///< client information
			bool i_anythingChanged
	)
	{
		if (cCommonData.verbosity_level > 3 || (i_anythingChanged && cCommonData.verbosity_level > 2))
		{
			std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++" << std::endl;
			std::cout << "INVADE (" << i_cClient->pid << ")" << std::endl;

			if (cCommonData.verbosity_level > 2)
			{
				std::cout << "  + pid: " << i_cClient->pid << std::endl;
				std::cout << "  + min_cores: " << i_cClient->constraint_min_cores << std::endl;
				std::cout << "  + max_cores: " << i_cClient->constraint_max_cores << std::endl;
				std::cout << "  + scalability graph: " << std::endl;

				printVec(i_cClient->hint_scalability_graph);
				std::cout << std::endl;

				std::cout << "Number of clients: " << clients.size() << std::endl;
				std::cout << std::endl;
			}
		}

//		printCurrentState("update resource distribution and send client message", i_cClient->client_id);

		/*
		 * send back mapping
		 */
		cMessages_Outgoing.msg_outgoing_sendInvadeAnswer(i_cClient, i_anythingChanged);
	}



	/**
	 * this method applies the new optimum for a client.
	 *
	 * 1) if more cores have to be assigned to a client:
	 *    search for free resources which can be assigned to the client.
	 *
	 * 2) if cores have to be released for a client:
	 *    release the client resources if less cores are assigned to the client.
	 *
	 * IMPORTANT: Releasing resources is allowed here only, because we assume
	 * a blocking communication! Thus the client releases the resources immediately.
	 */
	bool applyNewOptimumForClient(
			CClient &cClient,
			int i_optimal_vec_id
	)
	{
		if (cCommonData.verbosity_level > 5)
			std::cout << "APPLY NEW OPTIMUM" << std::endl;

		int delta_cores = optimal_cpu_distribution[i_optimal_vec_id] - cClient.number_of_assigned_cores;

		if (delta_cores == 0)
			return false;

		bool cores_changed = false;

		if (delta_cores > 0)
		{
			// try to increase number of assigned cores
			for (int i = cResources.max_cores-1; i >= 0; i--)
			{
				if (cResources.core_pids[i] == 0)
				{
					cores_changed = true;
					cResources.core_pids[i] = cClient.pid;
					cClient.assigned_cores.push_back(i);
					cClient.number_of_assigned_cores++;

					if (cCommonData.verbosity_level > 5)
						std::cout << " > Adding free core " << i << " to clients core list" << std::endl;

					delta_cores--;
					if (delta_cores == 0)
						break;
				}
			}

			// sort cores
			if (cores_changed)
				cClient.assigned_cores.sort();

			return cores_changed;
		}

		assert (delta_cores < 0);


		delta_cores = -delta_cores;

#if 0
		// start removing cores from the beginning

		// decrease number of assigned cores
		std::list<int>::iterator iter = cClient.assigned_cores.begin();

		for (int i = 0; i < delta_cores; i++)
		{
			assert(iter != cClient.assigned_cores.end());

			cores_changed = true;
			int core_id = *iter;
			core_pids[core_id] = 0;

			cClient.number_of_assigned_cores--;
			iter = cClient.assigned_cores.erase(iter);
		}

#else

		// start removing cores from the end

		// decrease number of assigned cores
		std::list<int>::iterator iter = cClient.assigned_cores.begin();

		for (int i = 0; i < cClient.number_of_assigned_cores-delta_cores; i++)
			iter++;

		for (int i = 0; i < delta_cores; i++)
		{
			assert(iter != cClient.assigned_cores.end());

			cores_changed = true;
			int core_id = *iter;
			cResources.core_pids[core_id] = 0;

			cClient.number_of_assigned_cores--;
			iter = cClient.assigned_cores.erase(iter);
		}

#endif

		return cores_changed;
	}




	/**
	 * ASYNCHRONOUS VERSION!
	 *
	 * this method applies the new optimum for a client in 2 steps.
	 *
	 * 1) if more cores have to be assigned to a client:
	 *   search for free resources which can be assigned to the client.
	 *   send an async invade answer
	 *
	 * 2) search for resources which have to be released.
	 *    Actually don't release this resources but forward the information that
	 *    this resources have to be released to the application
	 *
	 * IMPORTANT: Freeing resources is NOT allowed here. This only works with a blocking
	 * communication (see applyNewOptimumForClient)
	 */
	void applyNewOptimumForClientAsync(
			CClient *i_cClient,				///< client information
			int i_optimal_vec_id,			///< optimal vector id
			bool i_force_send_async_answer	///< anything was changed -> send the new affinities in any case
	)
	{
		if (i_cClient->reinvade_nonblocking_active)
			return;

		if (cCommonData.verbosity_level > 5)
		{
			std::cout << "Client: " << *i_cClient << ": APPLY NEW OPTIMUM (ASYNC):" << std::endl;
			std::cout << " + vec id: " << i_optimal_vec_id << std::endl;
			std::cout << " + optimum cores: " << optimal_cpu_distribution[i_optimal_vec_id] << std::endl;
			std::cout << " + currently assigned cores: " << i_cClient->number_of_assigned_cores << std::endl;
		}

		assert(i_optimal_vec_id < (int)optimal_cpu_distribution.size());

		int delta_cores = optimal_cpu_distribution[i_optimal_vec_id] - i_cClient->number_of_assigned_cores;


		/*********************************
		 * NOTHING TO DO?
		 */
		if (delta_cores == 0 && !i_force_send_async_answer)
		{

			if (cCommonData.verbosity_level > 5)
				std::cout << " > No change to get optimum is necessary" << std::endl;

			return;		// nothing to do
		}

		bool cores_changed = false;


		/*********************************
		 * ADD CORES?
		 */
		if (delta_cores > 0 || (i_force_send_async_answer && delta_cores == 0))
		{
			// try to increase number of assigned cores
			for (int i = 0; i < cResources.max_cores; i++)
			{
				if (cResources.core_pids[i] == 0)
				{
					cores_changed = true;
					cResources.core_pids[i] = i_cClient->pid;
					i_cClient->assigned_cores.push_back(i);
					i_cClient->number_of_assigned_cores++;

					if (cCommonData.verbosity_level > 5 )
						std::cout << " + applyNewOptimumForClientAsync: Adding free core " << i << " to clients core list" << std::endl;

					delta_cores--;
					if (delta_cores == 0)
						break;

					assert(0 <= i_cClient->assigned_cores.back());
					assert(i_cClient->assigned_cores.back() < cResources.max_cores);
				}
			}

			if (!cores_changed && !i_force_send_async_answer)
			{
				if (cCommonData.verbosity_level > 5)
					std::cout << " No free cores available" << std::endl;

				return;
			}

			// sort cores
			if (cores_changed)
				i_cClient->assigned_cores.sort();

			validateResources();


			/**
			 * SEND ASYNC REINVADE ANSWER to increase number of used resources
			 */
			SPMOMessage &m = *(SPMOMessage*)(cMessageQueueServer->msg_data_load_ptr);

			m.package_type = SPMOMessage::SERVER_REINVADE_NONBLOCKING;
			m.data.invade_answer.pid = i_cClient->pid;
			m.data.invade_answer.anythingChanged = true;
			m.data.invade_answer.number_of_cores = i_cClient->number_of_assigned_cores;
			assert(i_cClient->number_of_assigned_cores == (int)i_cClient->assigned_cores.size());

			int i = 0;
			for (std::list<int>::iterator iter = i_cClient->assigned_cores.begin(); iter != i_cClient->assigned_cores.end(); iter++)
			{
				m.data.invade_answer.affinity_array[i] = *iter;

				assert(0 <= m.data.invade_answer.affinity_array[i]);
				assert(m.data.invade_answer.affinity_array[i] < cResources.max_cores);

				i++;
			}

			if (cCommonData.verbosity_level <= -100)
			{
				std::cout << " + applyNewOptimumForClientAsync (request reinvade): Client " << i_cClient->client_id << " [" << i_cClient->pid << "]" << std::endl;
				std::cout << "   request to invade " << i_cClient->number_of_assigned_cores << " cores: ";
				for (int i = 0; i < i_cClient->number_of_assigned_cores; i++)
					std::cout << m.data.invade_answer.affinity_array[i] << " ";
				std::cout << std::endl;
			}

			m.data.invade_answer.seq_id = cCommonData.seq_id++;

			cMessageQueueServer->sendToClient(
					(size_t)&(m.data) - (size_t)&m +
					sizeof(m.data.invade_answer)+
					sizeof(int)*(i_cClient->number_of_assigned_cores-1),
					i_cClient->pid);

			i_cClient->reinvade_nonblocking_active = true;

			return;
		}



		/*********************************
		 * REMOVE CORES?
		 */
		validateResources();

		assert(delta_cores < 0);

		// number of cores which have to be freed
		delta_cores = -delta_cores;

		// decrease number of assigned cores
		std::list<int>::iterator iter = i_cClient->assigned_cores.begin();

		if (cCommonData.verbosity_level > 5 || cCommonData.verbosity_level <= -102)
			std::cout << " + ASYNC: Request removing " << delta_cores << " cores from application (" << i_cClient->assigned_cores.size() << " available)" << std::endl;

		for (int i = 0; i < delta_cores; i++)
		{
			assert(iter != i_cClient->assigned_cores.end());
			iter++;
		}

		SPMOMessage &m = *(SPMOMessage*)(cMessageQueueServer->msg_data_load_ptr);
		m.package_type = SPMOMessage::SERVER_REINVADE_NONBLOCKING;
		m.data.invade_answer.pid = i_cClient->pid;
		m.data.invade_answer.anythingChanged = true;
		m.data.invade_answer.number_of_cores = i_cClient->number_of_assigned_cores-delta_cores;

		// count number of remaining assigned cores
		int i = 0;
		for (; iter != i_cClient->assigned_cores.end(); iter++)
		{
			m.data.invade_answer.affinity_array[i] = *iter;

			assert(0 <= m.data.invade_answer.affinity_array[i]);
			assert(m.data.invade_answer.affinity_array[i] < cResources.max_cores);

			i++;
		}

		if (cCommonData.verbosity_level > 5 || cCommonData.verbosity_level <= -102)
			std::cout << " + ASYNC: Requesting to use only " << i << " cores (check: " << m.data.invade_answer.number_of_cores << ")" << std::endl;

		assert(i == m.data.invade_answer.number_of_cores);

		if (cCommonData.verbosity_level > 5 || cCommonData.verbosity_level <= -101)
		{
			std::cout << " + applyNewOptimumForClientAsync (request reinvade): Client " << i_cClient->client_id << " [" << i_cClient->pid << "]" << std::endl;
			std::cout << "   request reduction from " << i_cClient->number_of_assigned_cores << " to " << m.data.invade_answer.number_of_cores << " cores: ";
			for (int i = 0; i < m.data.invade_answer.number_of_cores; i++)
				std::cout << m.data.invade_answer.affinity_array[i] << " ";
			std::cout << std::endl;
		}

		m.data.invade_answer.seq_id = cCommonData.seq_id++;

		cMessageQueueServer->sendToClient(
				(size_t)&(m.data) - (size_t)&m +
				sizeof(m.data.invade_answer)+
				sizeof(int)*(m.data.invade_answer.number_of_cores-1),
				i_cClient->pid
			);

		i_cClient->reinvade_nonblocking_active = true;

		return;
	}



	/**
	 * output the current state
	 */
	void printCurrentState(
			const char *i_info_msg,
			int i_client_id
	)
	{
		if (cCommonData.verbosity_level >= 4)
		{
			std::cout << "=== PRINT CURRENT STATE ===" << std::endl;
			std::cout << "+ Client Mapping:" << std::endl;

			for (	std::list<CClient>::iterator iter = clients.begin();
					iter != clients.end();
					iter++)
			{
				CClient &c = *iter;

				std::cout << "  > Client " << c.client_id << " [" << c.pid << "] (" << c.number_of_assigned_cores << "):	";
				for (std::list<int>::iterator iter = c.assigned_cores.begin(); iter != c.assigned_cores.end(); iter++)
					std::cout << *iter << " ";
				std::cout << std::endl;
			}
		}


		if (cCommonData.verbosity_level >= 3)
		{
			std::cout << "CORE<->PIDs: " << "\t: ";
			for (int i = 0; i < cResources.max_cores; i++)
				std::cout << cResources.core_pids[i] << " ";
			std::cout << "\t" << i_info_msg << std::endl;
		}

		if (cCommonData.verbosity_level <= -99)
		{
			// see http://stdcxx.apache.org/doc/stdlibug/28-3.html for more information about formatting of std::cout
			std::ios_base::fmtflags original_flags = std::cout.flags();
			std::cout.setf(std::ios_base::left, std::ios_base::adjustfield);
			std::cout.width(10);
			std::cout << cStopwatch.getTimeSinceStart();
			std::cout.flags(original_flags);
			std::cout << ": [ ";

			for (int i = 0; i < cResources.max_cores; i++)
			{
				int pid = cResources.core_pids[i];

				if (pid == 0)
				{
					if (cCommonData.color_mode)
						std::cout << "\033[0;37m";

					std::cout << "0 ";
				}
				else
				{
					int clientVecId;
					CClient *c = searchClient(pid, &clientVecId);

					if (c == nullptr)
					{
						std::cout << "0 ";
					}
					else
					{
						if (cCommonData.color_mode)
							std::cout << "\033[0;" << (31+(c->client_id % 6)) << "m";

						std::cout << c->client_id << " ";
					}
				}

				if (i % 10 == 9 && i+1 != cResources.max_cores)
					std::cout << "\033[0;0m | ";
			}

			std::cout << "\033[0;0m]";

			std::cout << "\t" << i_info_msg;

			if (i_client_id >= 0)
				std::cout << " (" << i_client_id << ")";

			std::cout << std::endl;
		}

		if (cCommonData.verbosity_level <= -98)
		{
			for (std::list<CClient>::iterator i = clients.begin(); i != clients.end(); i++)
			{
				CClient &c = *i;

				std::cout << c.number_of_assigned_cores << "\t";
			}
			std::cout << std::endl;
		}

		if (cCommonData.verbosity_level >= 4)
		{
			std::cout << "+ Optimal point: ";
			for (size_t i = 0; i < optimal_cpu_distribution.size(); i++)
				std::cout << optimal_cpu_distribution[i] << " ";
			std::cout << std::endl;
			std::cout << "==========================" << std::endl;
		}
	}



	/**
	 * compute overall scalability among different clients for given resource distribution
	 */
	float computeScalability(
			std::vector<int> &o_sampling_point
	)
	{
		float scalability = 0;

		int i = 0;
		for (std::list<CClient>::iterator iter = clients.begin(); iter != clients.end(); iter++)
		{
			CClient &c = *iter;
			scalability += c.getScalability(o_sampling_point[i]);
			i++;
		}

		return scalability;
	}



	template <typename T>
	void printVec(std::vector<T> &i_vec)
	{
		for (size_t i = 0; i < i_vec.size(); i++)
			std::cout << i_vec[i] << ", ";
	}



	/**
	 * search for best global optimum
	 */
	void runGlobalOptimization()
	{
		int num_clients = clients.size();

		// resize optimal cpu distribution
		optimal_cpu_distribution.resize(num_clients, 1);

		/*
		 * initialize optimal cpu distribution for each client and setup distribution hint
		 */
		int a = 0;
		float sum_distribution_hint = 0;

		// reserve at least a single core per client
		int remaining_non_reserved_cores = cResources.max_cores-num_clients;

		int used_cores = 0;
		for (std::list<CClient>::iterator i = clients.begin(); i != clients.end(); i++)
		{
			CClient &c = *i;

			if (c.distribution_hint > 0)
				sum_distribution_hint += c.distribution_hint;

			if (c.constraint_min_cores <= 1)
			{
				optimal_cpu_distribution[a] = 1;
				remaining_non_reserved_cores--;
				used_cores++;
			}
			else
			{
				int delta = std::min(c.constraint_min_cores, remaining_non_reserved_cores);
				assert(delta >= 0);

				optimal_cpu_distribution[a] = delta;
				remaining_non_reserved_cores -= delta;
				used_cores += delta;
			}

			a++;
		}

		float inv_sum_distribution_hint = 0;
		if (sum_distribution_hint > 0)
			inv_sum_distribution_hint = (float)cResources.max_cores/sum_distribution_hint;

		float current_scalability = computeScalability(optimal_cpu_distribution);

#if 0
		std::cout << "START SEARCH POINT: ";
		for (int i = 0; i < clients.size(); i++)
			std::cout << optimal_cpu_distribution[i] << ", ";
		std::cout << std::endl;
#endif

		/*
		 * run at max. max_cores iterative searches
		 */
		for (; used_cores < cResources.max_cores; used_cores++)
		{
			float max_scalability_improvement = -1;
			int max_scalability_dir = -1;

			/**
			 * search for best scalability
			 */
			int ci = 0;
			for (std::list<CClient>::iterator iter = clients.begin(); iter != clients.end(); iter++)
			{
				CClient &c = *iter;

				if (c.distribution_hint > 0)
				{
					if (std::floor(c.distribution_hint*inv_sum_distribution_hint+0.5f) < optimal_cpu_distribution[ci])
						goto no_more_ressources;
				}

				if (c.constraint_max_cores > optimal_cpu_distribution[ci])
				{
					optimal_cpu_distribution[ci]++;
					float next_scalability = computeScalability(optimal_cpu_distribution);
					optimal_cpu_distribution[ci]--;

					float diff = next_scalability - current_scalability;

					if (diff > max_scalability_improvement)
					{
						max_scalability_improvement = diff;
						max_scalability_dir = ci;
					}
				}

			no_more_ressources:
				ci++;
			}

			if (max_scalability_dir == -1)
				break;

			optimal_cpu_distribution[max_scalability_dir]++;
		}
	}


	inline void validateResources()
	{
#if DEBUG
		int cores[1024];
		for (int i = 0; i < cResources.max_cores; i++)
			cores[i] = 0;

		for (std::list<CClient>::iterator iter = clients.begin(); iter != clients.end(); iter++)
		{
			CClient &c = *iter;

			for (	std::list<int>::iterator iter_cores = c.assigned_cores.begin();
					iter_cores != c.assigned_cores.end();
					iter_cores++
			)
			{
				int core_id = *iter_cores;
				if (cores[core_id] != 0)
				{
					std::cout << "RESOURCE CONFLICT DETECTED" << std::endl;
					cCommonData.verbosity_level = 99;
					printCurrentState("validateResources", -1);
					exit(-1);
				}

				cores[core_id] = c.client_id;
			}
		}
#endif
	}

	inline void printVerboseMsgIncomingHeader(
			const char *header_info
	)
	{
		if (cCommonData.verbosity_level >= 5 || cCommonData.verbosity_level <= -100)
			std::cout << "********************** MSG: " << header_info << " **********************" << std::endl;
	}



	/**
	 * setup client
	 */
	void msg_incoming_clientSetup(
			pid_t i_pid		///< client pid
	)
	{
		if (cCommonData.verbosity_level > 1)
		{
			std::cout << "CLIENT SETUP: adding client " << i_pid << std::endl;
			std::cout << " + TIMESTAMP: " << cStopwatch.getTimeSinceStart() << std::endl;
		}

		if (clients.empty() && cCommonData.start_time_first_client == 0)
		{
			cCommonData.start_time_first_client = cStopwatch.getTimeSinceStart();

			if (cCommonData.verbosity_level > 1)
			{
				std::cout << "START TIMESTAMP: " << cCommonData.start_time_first_client << std::endl;
			}
		}

		clients.push_back(CClient(i_pid, client_enumerator_id, cCommonData.verbosity_level));
		client_enumerator_id++;

		if (cCommonData.verbosity_level > 2)
			std::cout << " + sending ack to client " << i_pid << std::endl;

		// append one element to optimal cpu distribution
		optimal_cpu_distribution.push_back(0);

		// send ack
		cMessages_Outgoing.msg_outgoing_ack(i_pid);

		printCurrentState("client_setup", client_enumerator_id-1);
	}



	/**
	 * shutdown client
	 */
	void msg_incoming_clientShutdown(
			int i_pid,
			double client_shutdown_hint
	)
	{
		if (cCommonData.verbosity_level > 2)
			std::cout << cStopwatch.getTimeSinceStart() << " : CLIENT SHUTDOWN (" << i_pid << ")" << std::endl;

		CClient *c = searchClient(i_pid);

		if (c == 0)
		{
			std::cout << "CLIENT NOT FOUND! > ignoring" << std::endl;
			return;
		}

		c->releaseAllClientCoresAndFreeResources(cResources);
		int client_id = c->client_id;

		cCommonData.sum_client_shutdown_hint += client_shutdown_hint;

		clients.remove(*c);

		double end_time_last_client = cStopwatch.getTimeSinceStart();
		double time = end_time_last_client - cCommonData.start_time_first_client;

		cCommonData.sum_client_shutdown_hint_div_time = cCommonData.sum_client_shutdown_hint/time;

		if (clients.empty())
		{
			if (cCommonData.verbosity_level > 2)
			{
				std::cout << "END TIMESTAMP: " << end_time_last_client << std::endl;
				std::cout << "OVERALL TIME: " << time << std::endl;
			}
		}

		printClientsShutdownHint();

		cMessages_Outgoing.msg_outgoing_shutdown_ack(i_pid);

		runGlobalOptimization();

		searchAndSendDelayedACKs();

		sendAsyncReinvadeAnswers();

		printCurrentState("client_shutdown", client_id);
	}



	/**
	 * INVADE from C1:
	 *
	 * - search for client C1
	 * - update information for client C1
	 * - run global optimization for clients Cn
	 * - send update information to client C1
	 * - update client C1 resource distribution and send new resource distribution information to C1
	 * - send delayed ACKs
	 */
	int msg_incoming_invade(
			pid_t i_client_pid,					///< process id of client
			int i_min_cores,					///< minimum number of requested cores
			int i_max_cores,					///< maximum number of requested cores
			float i_distribution_hint,			///< distribution hint
			float i_scalability_graph[],		///< scalability graph
			int i_scalability_graph_size,		///< size of scalability graph
			bool i_update_resources_async = false	///< send upate message to client
	)
	{
		if (cCommonData.verbosity_level > 2)
			std::cout << cStopwatch.getTimeSinceStart() << "\t: " << i_client_pid << std::endl;

		int clientVecId;
		CClient *cClient = searchClient(i_client_pid, &clientVecId);

		if (cClient == 0)
		{
			if (cCommonData.verbosity_level > 3)
				std::cout << "client not found -> ignoring invade" << std::endl;
			return -1;
		}

		cClient->retreat_active = false;
		cClient->constraint_min_cores = i_min_cores;
		cClient->constraint_max_cores = i_max_cores;
		cClient->distribution_hint = i_distribution_hint;
		cClient->setScalabilityGraph(i_scalability_graph, i_scalability_graph_size);

		if (cCommonData.verbosity_level > 5 || cCommonData.verbosity_level <= -103)
		{
			std::cout << *cClient << ": invade - min/max cores: " << i_min_cores << "/" << i_max_cores << "   scalability: ";
			for (int i = 0; i < i_scalability_graph_size; i++)
				std::cout << i_scalability_graph[i] << " ";
			std::cout << std::endl;
		}

		runGlobalOptimization();

		if (i_update_resources_async)
		{
			applyNewOptimumForClientAsync(cClient, clientVecId, false);

			if (cClient->number_of_assigned_cores == 0)
			{
				// number of assigned cores == 0
				// => wait until resources are released

				if (cCommonData.verbosity_level >= 5)
					std::cout << "DELAYED INVADE ACK (" << cClient->pid << ") => wait until at least one core is released!" << std::endl;

				delayed_setup_acks_clients.push_back(cClient);

				searchAndSendDelayedACKs();
				return cClient->client_id;
			}
		}
		else
		{
			bool anythingChanged = applyNewOptimumForClient(*cClient, clientVecId);

			sendAsyncReinvadeAnswers();

			searchAndSendDelayedACKs();

			if (cClient->number_of_assigned_cores == 0)
			{
				// number of assigned cores == 0
				// => wait until resources are released

				std::cout << "DELAYED INVADE ACK (" << cClient->pid << ") => wait until at least one core is released!" << std::endl;
				delayed_setup_acks_clients.push_back(cClient);
				return cClient->client_id;
			}

			updateResourceDistributionAndSendClientMessage(cClient, anythingChanged);
		}

		if (!i_update_resources_async)
			printCurrentState("client_invade", cClient->client_id);

		return  cClient->client_id;
	}





	/**
	 * INVADE from C1:
	 *
	 * - search for client C1
	 * - update information for client C1
	 * - run global optimization for clients Cn
	 * - send update information to client C1
	 * - update client C1 resource distribution and send new resource distribution information to C1
	 * - send delayed ACKs
	 */
	void msg_incoming_invade_async(
			pid_t i_client_pid,				///< process id of client
			int i_min_cores,				///< minimum number of requested cores
			int i_max_cores,				///< maximum number of requested cores
			float i_distribution_hint,		///< distribution hint
			float i_scalability_graph[],	///< scalability graph
			int i_scalability_graph_size	///< size of scalability graph
	)
	{
		int client_id =
			msg_incoming_invade(
				i_client_pid,
				i_min_cores,
				i_max_cores,
				i_distribution_hint,
				i_scalability_graph,
				i_scalability_graph_size,
				true
			);

		sendAsyncReinvadeAnswers();

		if (cCommonData.verbosity_level <= -98)
		{
			printCurrentState("client_invade_async", client_id);
		}
	}



	/**
	 * update clients core utilization
	 */
	void msg_incoming_reinvade_ack_async(
			int i_client_pid,
			int i_num_cores,
			int *i_affinity_array
	)
	{
		/*
		 * sendAsyncInvadeAnswers
		 */
		int clientVecId;
		CClient *cClient = searchClient(i_client_pid, &clientVecId);

		if (cClient == 0)
		{
			std::cout << "Client with PID " << i_client_pid << " not found (ignored)!" << std::endl;
			return;
		}

		cClient->reinvade_nonblocking_active = false;

		if (cClient->retreat_active)
			return;

		if (cCommonData.verbosity_level > 5 || cCommonData.verbosity_level <= -100)
		{
			std::cout << " + " << *cClient << " msg_incoming_reinvade_ack_async:" << std::endl;

			std::cout << "   affinity array: ";
			for (int i = 0; i < i_num_cores; i++)
				std::cout << i_affinity_array[i] << " ";
			std::cout << std::endl;
		}

#if DEBUG
		// check whether all cores specified in the affinity_array are really available
		for (int i = 0; i < i_num_cores; i++)
		{
			int c = i_affinity_array[i];

			bool found = false;
			for (std::list<int>::iterator core_iter = cClient->assigned_cores.begin(); core_iter != cClient->assigned_cores.end(); core_iter++)
			{
				if (*core_iter == c)
				{
					found = true;
					break;
				}
			}

			if (!found)
			{
				std::cout << *cClient << " ERROR: core " << c << " not reserved!" << std::endl;
				assert(false);
				exit(-1);
			}
		}
#endif

		assert(cClient->number_of_assigned_cores == (int)cClient->assigned_cores.size());

		if (cCommonData.verbosity_level <= -100)
		{
			std::cout << "   + temporarily freeing client " << cClient->assigned_cores.size() << " cores: ";
			for (std::list<int>::iterator i = cClient->assigned_cores.begin(); i != cClient->assigned_cores.end(); i++)
				std::cout << *i << " ";

			std::cout << std::endl;
		}


		/*
		 * Step 1)
		 * Free all cores assigned to the client
		 *
		 * Runtime: O(n)
		 */
		for (std::list<int>::iterator i = cClient->assigned_cores.begin(); i != cClient->assigned_cores.end(); i++)
		{
			int core_id = *i;

			assert(core_id >= 0);
			assert(core_id < cResources.max_cores);

			// free the core
			cResources.core_pids[core_id] = 0;
		}


		/*
		 * clear core list assigned to the client
		 */
		cClient->assigned_cores.clear();


		/*
		 * Step 2)
		 * Assign cores to client
		 *
		 * Runtime: O(n)
		 *
		 * core ids are automatically ordered due to for loop over all cores
		 */
		for (int core_id = 0; core_id < i_num_cores; core_id++)
		{
			cClient->assigned_cores.push_back(i_affinity_array[core_id]);
			cResources.core_pids[i_affinity_array[core_id]] = cClient->pid;
		}

		if (cCommonData.verbosity_level <= -100)
		{
			std::cout << "   + async infected client cores: ";
			for (std::list<int>::iterator i = cClient->assigned_cores.begin(); i != cClient->assigned_cores.end(); i++)
				std::cout << *i << " ";

			std::cout << std::endl;
		}


		// update number of assigned cores
		cClient->number_of_assigned_cores = i_num_cores;

		searchAndSendDelayedACKs();

		printCurrentState("msg_incoming_reinvade_ack_async (before_async_reinvade)", cClient->client_id);

		sendAsyncReinvadeAnswers();

		printCurrentState("msg_incoming_reinvade_ack_async", cClient->client_id);
	}




	/**
	 * incoming reinvade message
	 */
	void msg_incoming_reinvade(
			pid_t i_client_pid			///< client PID
	)
	{
		int clientVecId;
		CClient *cClient = searchClient(i_client_pid, &clientVecId);

		if (cClient == 0)
		{
			if (cCommonData.verbosity_level > 3)
				std::cout << "client not found -> ignoring invade" << std::endl;
			return;
		}

		if (cClient->reinvade_nonblocking_active)
		{
			if (cCommonData.verbosity_level > 2)
				std::cout << "ignoring reinvade since async reinvade was already sent" << std::endl;

			cMessages_Outgoing.msg_outgoing_sendInvadeAnswer(cClient, false /* nothing changed */);
			return;
		}

		bool anythingChanged = applyNewOptimumForClient(*cClient, clientVecId);

		if (anythingChanged && cCommonData.verbosity_level > 2)
		{
//			std::cout << "++++++++++++++++++++++++++++++++++++++++++++++++" << std::endl;
			std::cout << " + TIMESTAMP: " << cStopwatch.getTimeSinceStart() << std::endl;
			std::cout << "  REINVADE for client " << cClient->client_id << std::endl;

			std::cout << "  + pid: " << i_client_pid << std::endl;
			std::cout << "  + min_cores: " << cClient->constraint_min_cores << std::endl;
			std::cout << "  + max_cores: " << cClient->constraint_max_cores << std::endl;
			std::cout << "  + scalability graph: " << std::endl;
			std::cout << "  + assigned cores: ";

			for (std::list<int>::iterator i = cClient->assigned_cores.begin(); i != cClient->assigned_cores.end(); i++)
				std::cout << *i << " ";
			std::cout << std::endl;


			printVec(cClient->hint_scalability_graph);
			std::cout << std::endl;

			std::cout << "Number of clients: " << clients.size() << std::endl;
			std::cout << std::endl;
		}

		if (	(anythingChanged && cCommonData.verbosity_level > 1)	||
				cCommonData.verbosity_level > 2
		)
			printCurrentState("reinvade", cClient->client_id);


		/*
		 * send back mapping
		 */
		cMessages_Outgoing.msg_outgoing_sendInvadeAnswer(cClient, anythingChanged);

		searchAndSendDelayedACKs();

		sendAsyncReinvadeAnswers();
	}


	/**
	 * RETREAT from C1:
	 *
	 * - search for data structure of client C1
	 * - release C1 resources
	 * - run global optimization
	 * - send ack to C1
	 * - send delayed ACKs
	 */
	void msg_incoming_retreat(
			pid_t i_client_pid		///< clients pid
	)
	{
		if (cCommonData.verbosity_level > 5)
			std::cout << "RETREAT (" << i_client_pid << ")" << std::endl;

		int i;
		CClient *cClient = searchClient(i_client_pid, &i);

		if (cClient == 0)
		{
			if (cCommonData.verbosity_level > 5)
				std::cout << "CLIENT (" << cClient->client_id << ") not found" << std::endl;
			return;
		}

		cClient->retreat_active = true;


		/*
		 * the special circumstance of `#cores == 0` can occur whenever an
		 * async invade was triggered => simply do nothing
		 */
		if (cClient->number_of_assigned_cores != 0)
		{
			// release all client cores except the first one!
			cClient->releaseAllClientCoresAndFreeResources(cResources, true);

			cClient->constraint_max_cores = 1;
			cClient->constraint_min_cores = 1;

			runGlobalOptimization();
		}

		cMessages_Outgoing.msg_outgoing_ack(i_client_pid);

		searchAndSendDelayedACKs();

		sendAsyncReinvadeAnswers();

		printCurrentState("retreat", cClient->client_id);
	}



	/**
	 * send shutdown message to ourself
	 */
	void selfShutdown()
	{
		CMessageQueueClient cMessageQueueClient(cCommonData.verbosity_level);

		SPMOMessage &m = *(SPMOMessage*)(cMessageQueueClient.msg_data_load_ptr);

		m.package_type = SPMOMessage::CLIENT_SERVER_SHUTDOWN;
		m.data.server_shutdown.seq_id = cCommonData.seq_id++;


		cMessageQueueClient.sendToServer(
				(size_t)&(m.data) - (size_t)&m +
				sizeof(m.data.server_shutdown)
			);
	}

	bool action()
	{
		validateResources();

		int len = cMessageQueueServer->receiveFromClient();

		if (len < 0)
		{
			std::cout << "empty message received" << std::endl;
			exit(-1);
		}

		if (cCommonData.verbosity_level >= 5)
			std::cout << " + TIMESTAMP: " << cStopwatch.getTimeSinceStart() << std::endl;

		SPMOMessage &m = *(SPMOMessage*)(cMessageQueueServer->msg_data_load_ptr);


		switch(m.package_type)
		{
			case SPMOMessage::CLIENT_SETUP:
				printVerboseMsgIncomingHeader("CLIENT SETUP");

				msg_incoming_clientSetup(
						m.data.client_setup.pid
					);
				break;

			case SPMOMessage::CLIENT_SHUTDOWN:
				printVerboseMsgIncomingHeader("CLIENT SHUTDOWN");

				msg_incoming_clientShutdown(
						m.data.client_shutdown.pid,
						m.data.client_shutdown.client_shutdown_hint
					);
				break;

			case SPMOMessage::CLIENT_RETREAT:
				printVerboseMsgIncomingHeader("CLIENT RETREAT");

				msg_incoming_retreat(
						m.data.retreat.pid
					);
				break;

			case SPMOMessage::CLIENT_INVADE:
				printVerboseMsgIncomingHeader("CLIENT INVADE");

				msg_incoming_invade(
						m.data.invade.pid,
						m.data.invade.min_cpus,
						m.data.invade.max_cpus,
						m.data.invade.distribution_hint,
						m.data.invade.scalability_graph,
						m.data.invade.scalability_graph_size
					);
				break;

			case SPMOMessage::CLIENT_INVADE_NONBLOCKING:
				printVerboseMsgIncomingHeader("CLIENT INVADE NON-BLOCKING");

				msg_incoming_invade_async(
						m.data.invade.pid,
						m.data.invade.min_cpus,
						m.data.invade.max_cpus,
						m.data.invade.distribution_hint,
						m.data.invade.scalability_graph,
						m.data.invade.scalability_graph_size
					);
				break;

			case SPMOMessage::CLIENT_REINVADE_ACK_NONBLOCKING:
				printVerboseMsgIncomingHeader("CLIENT REINVADE ACK NON BLOCKING");

				msg_incoming_reinvade_ack_async(
						m.data.reinvade_ack_async.pid,
						m.data.reinvade_ack_async.number_of_cores,
						m.data.reinvade_ack_async.affinity_array
					);
				break;

			case SPMOMessage::CLIENT_REINVADE:
				printVerboseMsgIncomingHeader("CLIENT REINVADE");

				msg_incoming_reinvade(
						m.data.reinvade.pid
					);
				break;


			case SPMOMessage::CLIENT_SERVER_SHUTDOWN:
				std::cout << "CLIENT SERVER SHUTDOWN" << std::endl;
				printVerboseMsgIncomingHeader("CLIENT SERVER SHUTDOWN");

				return false;
				break;

			default:
				std::cout << "UNKNOWN MESSAGE TYPE: " << m.package_type << std::endl;
				break;
		}

		validateResources();
		return true;
	}
};

#endif /* CWORLDSCHEDULER_HPP_ */
