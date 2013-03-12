/*
 * CPMO.hpp
 *
 *  Created on: Mar 28, 2012
 *      Author: Martin Schreiber <martin.schreiber@in.tum.de>
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
	int num_computing_threads;

	/**
	 * shutdown hint forwarded to world scheduler during shutdown
	 */
	double client_shutdown_hint;

	/**
	 * current id of process
	 */
	pid_t this_pid;

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
	 * shutdown is currently in progress
	 */
	bool shutdown_in_progress;

	/**
	 * retreat is currently in progress
	 * => don't send anymore any async ACK's
	 */
	bool retreat_in_progress;

	/**
	 * singleton for ctrl-c handler
	 */
	static CPMO *cPmoSingleton;

	/**
	 * level of verbosity
	 */
	int verbose_level;

	/**
	 * last seq id
	 */
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

//		std::cout << "HANDLE INVADE ANSWER: setNumberOfThreads" << std::endl;
		// update number of threads
		setNumberOfThreads(sPMOMessage->data.invade_answer.number_of_cores);

		// update the affinities
//		std::cout << "HANDLE INVADE ANSWER: setAffinities" << std::endl;
		setAffinities(sPMOMessage->data.invade_answer.affinity_array, sPMOMessage->data.invade_answer.number_of_cores);

//		std::cout << "HANDLE INVADE ANSWER: END" << std::endl;
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

		// update number of threads
		setNumberOfThreads(sPMOMessage->data.invade_answer.number_of_cores);

		// update the affinities
		setAffinities(sPMOMessage->data.invade_answer.affinity_array, sPMOMessage->data.invade_answer.number_of_cores);

		// send update to server
		sPMOMessage->package_type = SPMOMessage::CLIENT_REINVADE_ACK_NONBLOCKING;

		cMessageQueue->sendToServer(
				(size_t)&(sPMOMessage->data.reinvade_ack_async) - (size_t)sPMOMessage +
				sizeof(sPMOMessage->data.reinvade_ack_async) +
				sizeof(int)*(sPMOMessage->data.reinvade_ack_async.number_of_cores-1)
			);

		return true;
	}

	bool wait_for_ack;

#if DEBUG
	bool setup_executed;
#endif

public:
	/**
	 * constructor
	 */
	CPMO(
			int i_verbose_level = 0,		///< verbosity
			bool i_wait_for_ack = true		///< not waiting for an ack can be important for MPI parallelized programs since they run into a deadlock when not being able to give away cores during an MPI_barrier()
	)	:
		num_computing_threads(0),
		client_shutdown_hint(0),
		shutdown_in_progress(false),
		retreat_in_progress(false),
		verbose_level(i_verbose_level),
		last_seq_id(0),
		wait_for_ack(i_wait_for_ack)
#if DEBUG
		,
		setup_executed(false)
#endif
	{
		// store PID
		this_pid = getpid();

		if (verbose_level > 1)
			std::cout << "CLIENT PID: " << this_pid << std::endl;

		// send setup message
		if (verbose_level > 1)
			std::cout << "MSG SND: CLIENT SETUP" << std::endl;

		// allocate message queue
		cMessageQueue = new CMessageQueueClient;

		// get pointer to payload
		sPMOMessage = (SPMOMessage*)(cMessageQueue->msg_data_load_ptr);

		// setup client setup message
		sPMOMessage->package_type = SPMOMessage::CLIENT_SETUP;
		sPMOMessage->data.client_setup.pid = this_pid;
		sPMOMessage->data.client_setup.seq_id = last_seq_id;

		// send message to server
		cMessageQueue->sendToServer(
				(size_t)&(sPMOMessage->data.client_setup) - (size_t)sPMOMessage +
				sizeof(sPMOMessage->data.client_setup)
			);

		CPMO::cPmoSingleton = this;

		signal(SIGABRT, &myCTRLCHandler);
		signal(SIGTERM, &myCTRLCHandler);
		signal(SIGINT, &myCTRLCHandler);
	}


	void setup()
	{
#if DEBUG
		assert(!setup_executed);
		setup_executed = true;
#endif

		if (wait_for_ack)
		{
			std::cout << "WAIT FOR ACK" << std::endl;

			// WAIT FOR ACK when comm consists only out of a single MPI node, or if MPI is activated
			msg_recv_message_loop_blocking(SPMOMessage::SERVER_ACK);


			std::cout << "WAIT FOR ACK END" << std::endl;
		}
	}


	/**
	 * deconstructor
	 */
	virtual ~CPMO()
	{
#if DEBUG
		if (!setup_executed)
			std::cerr << "setup not executed" << std::endl;
#endif

		shutdown();
	}


	/**
	 * shutdown simulation
	 */
	void shutdown()
	{
#if DEBUG
		if (!setup_executed)
			std::cerr << "setup not executed" << std::endl;
#endif

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
		msg_recv_message_loop_blocking(SPMOMessage::CLIENT_ACK_SHUTDOWN);

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
//				std::cout << this_pid << ": Processing message type SERVER_QUIT" << std::endl;
				std::cerr << "QUIT requested from server" << std::endl;
				exit(-1);
				return false;

			case SPMOMessage::CLIENT_ACK_SHUTDOWN:
//				std::cout << this_pid << ": Processing message type CLIENT_ACK_SHUTDOWN" << std::endl;
				if (sPMOMessage->data.ack_quit.seq_id <= last_seq_id)
					std::cerr << "SEQ ID wrong " << sPMOMessage->data.ack_quit.seq_id << " <= " << last_seq_id << std::endl;

				assert(sPMOMessage->data.ack_quit.seq_id > last_seq_id);
				last_seq_id = sPMOMessage->data.ack_quit.seq_id;

				// update client shutdown hint
				std::cout << "SUM CLIENT SHUTDOWN HINT: " << sPMOMessage->data.ack_quit.client_shutdown_hint << std::endl;
				std::cout << "SUM CLIENT SHUTDOWN HINT DIV TIME: " << sPMOMessage->data.ack_quit.client_shutdown_hint_div_time << std::endl;
				return false;

			case SPMOMessage::SERVER_INVADE_ANSWER:
//				std::cout << this_pid << ": Processing message type SERVER_INVADE_ANSWER" << std::endl;
				if (sPMOMessage->data.invade_answer.seq_id <= last_seq_id)
					std::cerr << "SEQ ID wrong " << sPMOMessage->data.invade_answer.seq_id << " <= " << last_seq_id << std::endl;

				assert(sPMOMessage->data.invade_answer.seq_id > last_seq_id);
				last_seq_id = sPMOMessage->data.invade_answer.seq_id;

				return handleInvadeAnswer();

			case SPMOMessage::SERVER_REINVADE_NONBLOCKING:
//				std::cout << this_pid << ": Processing message type SERVER_REINVADE_NONBLOCKING" << std::endl;
				if (sPMOMessage->data.invade.seq_id <= last_seq_id)
					std::cerr << "SEQ ID wrong " << sPMOMessage->data.invade.seq_id << " <= " << last_seq_id << std::endl;

				assert(sPMOMessage->data.invade.seq_id > last_seq_id);
				last_seq_id = sPMOMessage->data.invade_answer.seq_id;

				if (retreat_in_progress)
					return false;

				return handleReinvadeNonblocking();

			case SPMOMessage::SERVER_ACK:
//				std::cout << this_pid << ": Processing message type SERVER_ACK" << std::endl;
				if (sPMOMessage->data.ack.seq_id <= last_seq_id)
					std::cerr << "SEQ ID wrong " << sPMOMessage->data.ack.seq_id << " <= " << last_seq_id << std::endl;

				assert(sPMOMessage->data.ack.seq_id > last_seq_id);
				last_seq_id = sPMOMessage->data.ack.seq_id;

				if (getNumberOfThreads() == 0)
					setNumberOfThreads(1);

				return false;

			default:
				std::cerr << "UNKNOWN MESSAGE TYPE " << sPMOMessage->package_type << std::endl;
				assert(false);
				exit(-1);
				return false;
		}
	}



	bool msg_recv_message_loop_blocking(
			unsigned long long stop_message = SPMOMessage::DUMMY
	)
	{
		while (cMessageQueue->receiveFromServer(this_pid) >= 0)
		{
//			std::cout << "WAIT FOR MSG" << std::endl;
			bool resources_changed = msg_recv_message_innerloop();

//			std::cout << "MSG RECEIVED and PROCESSED" << std::endl;

			if (sPMOMessage->package_type == stop_message)
				return resources_changed;

			if (stop_message == SPMOMessage::DUMMY)
				return resources_changed;
		}

//		std::cout << "msg_recv_message_loop_blocking FIN" << std::endl;
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
	/**
	 * invade computation resources
	 */
	bool invade_blocking(
			int i_min_cpus,								///< minimum number of cores
			int i_max_cpus,								///< maximum number of cores
			int i_scalability_graph_size = 0,			///< size of scalability graph
			const float *i_scalability_graph = nullptr,	///< scalability graph
			float i_distribution_hint = -1.0f			///< distribution hint
	)
	{
//		std::cout << this_pid << ": invade_blocking START" << std::endl;

		/*
		 * TODO: probably another handling is necessary to assure that all
		 */
		retreat_in_progress = false;

		msg_send_invade(
				i_min_cpus,
				i_max_cpus,
				i_scalability_graph_size,
				i_scalability_graph,
				i_distribution_hint
			);

		bool retval = msg_recv_message_loop_blocking(SPMOMessage::SERVER_INVADE_ANSWER);

//		std::cout << "invade_blocking END" << std::endl;
		return retval;
	}



public:
	/**
	 * invade computation resources (nonblocking)
	 */
	void invade_nonblocking(
			int i_min_cpus,		///< minimum number of cores
			int i_max_cpus,		///< maximum number of cores
			int i_scalability_graph_size = 0,				///< size of scalability graph
			const float *i_scalability_graph = nullptr,	///< scalability graph
			float i_distribution_hint = -1.0f				///< distribution hint
	)
	{
		assert(setup_executed);

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
		assert(setup_executed);

		return invade_blocking(
				i_min_cpus,
				i_max_cpus,
				scalability_graph.size(),
				scalability_graph.data()
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
			int i_size,
			float *i_scalability_graph
	)
	{
		assert(setup_executed);

		return invade_blocking(
				i_min_cpus,
				i_max_cpus,
				i_size,
				i_scalability_graph
			);
	}



	/**
	 * reinvade
	 */
	bool reinvade_blocking()
	{
		assert(setup_executed);

		sPMOMessage->package_type = SPMOMessage::CLIENT_REINVADE;

		sPMOMessage->data.invade.pid = this_pid;

		// send setup message
		cMessageQueue->sendToServer(
									(size_t)&(sPMOMessage->data) - (size_t)sPMOMessage +
									sizeof(sPMOMessage->data.invade.pid)
								);

		return msg_recv_message_loop_blocking(SPMOMessage::SERVER_INVADE_ANSWER);
	}



	/**
	 * nonblocking msgsrv based reinvade
	 */
	bool reinvade_nonblocking()
	{
		assert(setup_executed);

		// test for a new message
		return msg_recv_message_loop_nonblocking(SPMOMessage::DUMMY);
	}



	/**
	 * retreat all resources
	 */
	void retreat(bool dontWaitForAck = false)
	{
		assert(setup_executed);

		retreat_in_progress = true;

		sPMOMessage->package_type = SPMOMessage::CLIENT_RETREAT;
		sPMOMessage->data.retreat.pid = this_pid;

		// send setup message
		cMessageQueue->sendToServer(
									(size_t)&(sPMOMessage->data) - (size_t)sPMOMessage +
									sizeof(sPMOMessage->data.retreat)
								);

		if (dontWaitForAck)
			return;

		msg_recv_message_loop_blocking(SPMOMessage::SERVER_ACK);
	}
};


CPMO *CPMO::cPmoSingleton = 0;

#endif /* CPMO_HPP_ */
