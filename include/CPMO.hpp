/*
 * CPMO.hpp
 *
 *  Created on: Mar 28, 2012
 *      Author: schreibm
 */

#ifndef CPMO_HPP_
#define CPMO_HPP_


#include <iostream>
#include <pthread.h>
#include <sched.h>
#include <vector>
#include <signal.h>

#include "../include/CMessageQueueClient.hpp"
#include "../include/SPMOMessage.hpp"



/*
 * invasive client interface
 */
class CPMO
{
public:
	/**
	 * number of threads available in whole system
	 */
	int max_threads;

	/**
	 * current number of running threads
	 */
	int num_running_threads;

	/**
	 * shutdown hint forwarded to world scheduler during shutdown
	 */
	double client_shutdown_hint;

private:
	/**
	 * message queue handler
	 */
	CMessageQueueClient *cMessageQueue;

	/**
	 * pointer to message encoding
	 */
	SPMOMessage *sPMOMessage;

	/**
	 * current id of process
	 */
	pid_t this_pid;

	/**
	 * shutdown is currently in progress
	 */
	bool shutdown_in_progress;


	/**
	 * singleton for ctrl-c handler
	 */
	static CPMO *cPmoSingleton;

	/**
	 * level of verbosity
	 */
	int verbose_level;

//	int affinities[1024];

	unsigned long last_seq_id;


	/**
	 * CTRL-C handler
	 */
	static void myCTRLCHandler(int s)
	{
	    signal(SIGABRT, SIG_DFL);
	    signal(SIGTERM, SIG_DFL);
		signal(SIGINT, SIG_DFL);

		// send retreat signal without waiting for ack
		CPMO::cPmoSingleton->retreat(true);

		CPMO::cPmoSingleton->client_shutdown_hint = 1;

		// don't delete singleton due to TBB
		CPMO::cPmoSingleton->shutdown();

//		delete cPmoSingleton;
		exit(-1);
	}



private:
	/**
	 * shrink/grow number of threads to use for parallel regions
	 */
	virtual void setNumberOfThreads(int n) = 0;

	/**
	 * return the number of running threads
	 */
	virtual int getNumberOfThreads() = 0;

	/**
	 * update number of used threads
	 */
	virtual void delayedUpdateNumberOfThreads() = 0;

	/**
	 * return the maximum allowed number of running threads
	 */
	virtual int getMaxNumberOfThreads() = 0;


	/**
	 * set affinities for "num_running_threads" threads
	 */
	virtual void setAffinities(
			const int *i_cpu_affinities,
			int i_number_of_cpu_affinities
	) = 0;


	/**
	 * handle invade message from server and return true if anything has to be changed
	 */
	bool handleInvadeAnswer()
	{
		if (shutdown_in_progress)
			return false;

		// nothing to do if nothing was changed!
		if (!sPMOMessage->data.invade_answer.anythingChanged)
			return false;

		if (verbose_level > 3)
			std::cout << this_pid << ": CHANGING RESOURCES" << std::endl;

		assert(sPMOMessage->data.invade_answer.number_of_cores > 0);

		// update number of threads
		setNumberOfThreads(sPMOMessage->data.invade_answer.number_of_cores);

		// update the affinities
		setAffinities(sPMOMessage->data.invade_answer.affinity_array, sPMOMessage->data.invade_answer.number_of_cores);


/*
		std::cout << "handleInvadeAnswer" << std::endl;
		std::cout << " num cores: " << sPMOMessage->data.invade_answer.number_of_cores << std::endl;
		for (int i = 0; i < sPMOMessage->data.invade_answer.number_of_cores; i++)
			std::cout << sPMOMessage->data.invade_answer.affinity_array[i] << " ";
		std::cout << std::endl;
*/

/*
		// update affinity cache
		for (int i = 0; i < sPMOMessage->data.invade_answer.number_of_cores; i++)
			affinities[i] = sPMOMessage->data.invade_answer.affinity_array[i];
*/
		return true;
	}


	/**
	 * handle reinvade message from server and return true if anything has to be changed
	 */
	bool handleReinvadeNonblocking()
	{
		if (shutdown_in_progress)
			return false;

		if (verbose_level > 3)
			std::cout << this_pid << ": CHANGING RESOURCES (NONBLOCKING)" << std::endl;

		assert(sPMOMessage->data.invade_answer.number_of_cores > 0);

#if 0
		if (sPMOMessage->data.invade_answer.number_of_cores == getNumberOfThreads())
		{
			int i = 0;
			for (; i < sPMOMessage->data.invade_answer.number_of_cores; i++)
			{
				if (affinities[i] != sPMOMessage->data.invade_answer.affinity_array[i])
					break;
			}

			// nothing to change
			if (i == sPMOMessage->data.invade_answer.number_of_cores)
				return;
		}
#endif

		// update number of threads
		setNumberOfThreads(sPMOMessage->data.invade_answer.number_of_cores);

		// update the affinities
		setAffinities(sPMOMessage->data.invade_answer.affinity_array, sPMOMessage->data.invade_answer.number_of_cores);

		// send update to server
		sPMOMessage->package_type = SPMOMessage::CLIENT_REINVADE_ACK_NONBLOCKING;
#if 0
		// update affinity cache
		for (int i = 0; i < sPMOMessage->data.invade_answer.number_of_cores; i++)
			affinities[i] = sPMOMessage->data.invade_answer.affinity_array[i];
#endif
/*
		std::cout << getpid() << ": ";
		for (int i = 0; i < sPMOMessage->data.invade_answer.number_of_cores; i++)
			std::cout << sPMOMessage->data.invade_answer.affinity_array[i] << " ";
		std::cout << std::endl;
*/

		cMessageQueue->sendToServer(
				(size_t)&(sPMOMessage->data.reinvade_ack_async) - (size_t)sPMOMessage +
				sizeof(sPMOMessage->data.reinvade_ack_async) +
				sizeof(int)*(sPMOMessage->data.reinvade_ack_async.number_of_cores-1)
			);

		return false;
	}



public:
	/**
	 * constructor
	 */
	CPMO(
			int i_verbose_level = 0		///< verbosity
	)
	{
		verbose_level = i_verbose_level;
		client_shutdown_hint = 0;
		last_seq_id = 0;
		shutdown_in_progress = false;
		this_pid = getpid();

		if (verbose_level > 1)
			std::cout << "CLIENT PID: " << this_pid << std::endl;

		// send setup message
		if (verbose_level > 1)
			std::cout << "MSG SND: CLIENT SETUP" << std::endl;

		cMessageQueue = new CMessageQueueClient;

		sPMOMessage = (SPMOMessage*)(cMessageQueue->msg_data_load_ptr);

		/*
		 * send client setup message
		 */
		sPMOMessage->package_type = SPMOMessage::CLIENT_SETUP;
		sPMOMessage->data.client_setup.pid = this_pid;
		sPMOMessage->data.client_setup.seq_id = last_seq_id;

		cMessageQueue->sendToServer(
				(size_t)&(sPMOMessage->data.client_setup) - (size_t)sPMOMessage +
				sizeof(sPMOMessage->data.client_setup)
			);
/*
		for (int i = 0; i < 1024; i++)
			affinities[i] = -1;
*/
		// WAIT FOR ACK
		msg_recv_message_loop(SPMOMessage::SERVER_ACK);

		CPMO::cPmoSingleton = this;

	    signal(SIGABRT, &myCTRLCHandler);
	    signal(SIGTERM, &myCTRLCHandler);
		signal(SIGINT, &myCTRLCHandler);
	}


	/**
	 * deconstructor
	 */
	virtual ~CPMO()
	{
		shutdown();
	}

	void shutdown()
	{
		shutdown_in_progress = true;

	    signal(SIGABRT, SIG_DFL);
	    signal(SIGTERM, SIG_DFL);
		signal(SIGINT, SIG_DFL);

		// send setup message
		sPMOMessage->package_type = SPMOMessage::CLIENT_SHUTDOWN;
		sPMOMessage->data.client_shutdown.pid = this_pid;
		sPMOMessage->data.client_shutdown.client_shutdown_hint = client_shutdown_hint;

		cMessageQueue->sendToServer(
				(size_t)&(sPMOMessage->data) - (size_t)sPMOMessage +
				sizeof(sPMOMessage->data.client_shutdown)
			);

		// WAIT FOR ACK
		msg_recv_message_loop(SPMOMessage::CLIENT_ACK_SHUTDOWN);

		if (cMessageQueue != nullptr)
		{
			delete cMessageQueue;
			cMessageQueue = nullptr;
		}
	}



private:
	void msg_send_invade(
			int i_min_cpus,				///< minimum number of cores
			int i_max_cpus,				///< maximum number of cores
			int i_scalability_graph_size,		///< size of scalability graph
			const float *i_scalability_graph,	///< scalability graph
			float i_distribution_hint,			///< distribution hint
			SPMOMessage::MSG_TYPE i_package_type = SPMOMessage::CLIENT_INVADE
	)
	{
		assert(i_min_cpus > 0);
		assert(i_max_cpus > 0);

		sPMOMessage->package_type = i_package_type;

		// PID
		sPMOMessage->data.invade.pid = this_pid;

		// MIN/MAX cores
		sPMOMessage->data.invade.min_cpus = i_min_cpus;
		sPMOMessage->data.invade.max_cpus = i_max_cpus;

		// DISTRIBUTION HINT
		sPMOMessage->data.invade.distribution_hint = i_distribution_hint;

		// SCALABILITY GRAPH
		sPMOMessage->data.invade.scalability_graph_size = i_scalability_graph_size;
		for (int i = 0; i < i_scalability_graph_size; i++)
			sPMOMessage->data.invade.scalability_graph[i] = i_scalability_graph[i];

		// send setup message
		cMessageQueue->sendToServer(
						(size_t)&(sPMOMessage->data) - (size_t)sPMOMessage +
						sizeof(sPMOMessage->data.invade) +
						sizeof(float)*(i_scalability_graph_size-1)
					);
	}



	/**
	 * handle message
	 *
	 * return true if the resources were changed
	 */
	bool msg_recv_message_innerloop()
	{
		switch(sPMOMessage->package_type)
		{
			case SPMOMessage::SERVER_QUIT:
				std::cerr << "QUIT requested from server" << std::endl;
				exit(-1);
				return false;

			case SPMOMessage::CLIENT_ACK_SHUTDOWN:
				if (sPMOMessage->data.ack_quit.seq_id <= last_seq_id)
					std::cerr << "SEQ ID wrong " << sPMOMessage->data.ack_quit.seq_id << " <= " << last_seq_id << std::endl;

				assert(sPMOMessage->data.ack_quit.seq_id > last_seq_id);
				last_seq_id = sPMOMessage->data.ack_quit.seq_id;

				// update client shutdown hint
				std::cout << "SUM CLIENT SHUTDOWN HINT: " << sPMOMessage->data.ack_quit.client_shutdown_hint << std::endl;
				std::cout << "SUM CLIENT SHUTDOWN HINT DIV TIME: " << sPMOMessage->data.ack_quit.client_shutdown_hint_div_time << std::endl;
				return false;

			case SPMOMessage::SERVER_INVADE_ANSWER:
				if (sPMOMessage->data.invade_answer.seq_id <= last_seq_id)
					std::cerr << "SEQ ID wrong " << sPMOMessage->data.invade_answer.seq_id << " <= " << last_seq_id << std::endl;

				assert(sPMOMessage->data.invade_answer.seq_id > last_seq_id);
				last_seq_id = sPMOMessage->data.invade_answer.seq_id;

				return handleInvadeAnswer();

			case SPMOMessage::SERVER_REINVADE_NONBLOCKING:
				if (sPMOMessage->data.invade.seq_id <= last_seq_id)
					std::cerr << "SEQ ID wrong " << sPMOMessage->data.invade.seq_id << " <= " << last_seq_id << std::endl;

				assert(sPMOMessage->data.invade.seq_id > last_seq_id);
				last_seq_id = sPMOMessage->data.invade_answer.seq_id;

				return handleReinvadeNonblocking();

			case SPMOMessage::SERVER_ACK:
				if (sPMOMessage->data.ack.seq_id <= last_seq_id)
					std::cerr << "SEQ ID wrong " << sPMOMessage->data.ack.seq_id << " <= " << last_seq_id << std::endl;

				assert(sPMOMessage->data.ack.seq_id > last_seq_id);
				last_seq_id = sPMOMessage->data.ack.seq_id;

				return false;

			default:
				std::cerr << "UNKNOWN MESSAGE TYPE " << sPMOMessage->package_type << std::endl;
				assert(false);
				exit(-1);
				return false;
		}
	}



	bool msg_recv_message_loop(
			unsigned long long stop_message = SPMOMessage::DUMMY
	)
	{
		while (cMessageQueue->receiveFromServer(this_pid) >= 0)
		{
			bool resources_changed = msg_recv_message_innerloop();

			if (sPMOMessage->package_type == stop_message)
				return resources_changed;

			if (stop_message == SPMOMessage::DUMMY)
				return resources_changed;
		}

		return false;
	}



	bool msg_recv_message_loop_nonblocking(
			unsigned long long stop_message = SPMOMessage::DUMMY
	)
	{
		while (cMessageQueue->receiveNonblockingFromServer(this_pid) >= 0)
		{
			bool resources_changed = msg_recv_message_innerloop();

			if (sPMOMessage->package_type == stop_message)
				return resources_changed;

			if (stop_message == SPMOMessage::DUMMY)
				return resources_changed;
		}

		return false;
	}


public:
	bool invade(
			int i_min_cpus,		///< minimum number of cores
			int i_max_cpus,		///< maximum number of cores
			int i_scalability_graph_size = 0,				///< size of scalability graph
			const float *i_scalability_graph = nullptr,	///< scalability graph
			float i_distribution_hint = -1.0f				///< distribution hint
	)
	{
		msg_send_invade(
				i_min_cpus,
				i_max_cpus,
				i_scalability_graph_size,
				i_scalability_graph,
				i_distribution_hint
			);

		return msg_recv_message_loop(SPMOMessage::SERVER_INVADE_ANSWER);
	}




public:
	void invade_nonblocking(
			int i_min_cpus,		///< minimum number of cores
			int i_max_cpus,		///< maximum number of cores
			int i_scalability_graph_size = 0,				///< size of scalability graph
			const float *i_scalability_graph = nullptr,	///< scalability graph
			float i_distribution_hint = -1.0f				///< distribution hint
	)
	{
		msg_send_invade(
				i_min_cpus,
				i_max_cpus,
				i_scalability_graph_size,
				i_scalability_graph,
				i_distribution_hint,
				SPMOMessage::CLIENT_INVADE_NONBLOCKING
			);
	}



	/**
	 * invade with scalability graph
	 *
	 * this also means an implicit retreat
	 */
public:
	bool invade(
			int i_min_cpus,
			int i_max_cpus,
			std::vector<float> &scalability_graph
	)
	{
		return invade(
				i_min_cpus,
				i_max_cpus,
				scalability_graph.size(),
				scalability_graph.data()
			);
	}



	/**
	 * reinvade
	 */
	bool reinvade()
	{
		sPMOMessage->package_type = SPMOMessage::CLIENT_REINVADE;

		sPMOMessage->data.invade.pid = this_pid;

		// send setup message
		cMessageQueue->sendToServer(
									(size_t)&(sPMOMessage->data) - (size_t)sPMOMessage +
									sizeof(sPMOMessage->data.invade.pid)
								);

		return msg_recv_message_loop(SPMOMessage::SERVER_INVADE_ANSWER);
	}



	/**
	 * nonblocking msgsrv based reinvade
	 */
	bool reinvade_nonblocking()
	{
		// test for a new message
		return msg_recv_message_loop_nonblocking(SPMOMessage::DUMMY);
	}



	/**
	 * retreat all resources
	 */
	void retreat(bool dontWaitForAck = false)
	{
		sPMOMessage->package_type = SPMOMessage::CLIENT_RETREAT;
		sPMOMessage->data.retreat.pid = this_pid;

		// send setup message
		cMessageQueue->sendToServer(
									(size_t)&(sPMOMessage->data) - (size_t)sPMOMessage +
									sizeof(sPMOMessage->data.retreat)
								);

		if (dontWaitForAck)
			return;

		msg_recv_message_loop(SPMOMessage::SERVER_ACK);
	}
};


CPMO *CPMO::cPmoSingleton = 0;

#endif /* CPMO_HPP_ */
