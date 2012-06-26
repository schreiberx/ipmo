/*
 * CMessageQueue.hpp
 *
 *  Created on: Mar 28, 2012
 *      Author: schreibm
 */

#ifndef SPMOMESSAGE_HPP
#define SPMOMESSAGE_HPP

struct SPMOMessage
{
public:
	enum MSG_TYPE
	{
		CLIENT_SETUP = 1,		///< setup ipmo client
		CLIENT_SHUTDOWN,		///< shutdown ipmo client
		CLIENT_ACK_SHUTDOWN,	///< client ack for quit

		CLIENT_SERVER_SHUTDOWN,		///< client requests to shutdown ipmo server (useful for MPI)

		CLIENT_INVADE,				///< invade
		SERVER_INVADE_ANSWER,		///< invade answer from server

		CLIENT_INVADE_NONBLOCKING,	///< async invade:
									///< (send new constraints/hints to system)

		SERVER_REINVADE_NONBLOCKING,	///< async reinvade from server:
										///< (check for resource update information)
										///< use package format of SERVER_INVADE_ANSWER (invade_answer)

		CLIENT_REINVADE_ACK_NONBLOCKING,	///< async reinvade update information from client to server
									///< (send information about which resources were updated)

		CLIENT_REINVADE,			///< reinvade based on previous constraints / hints

		CLIENT_RETREAT,				///< retreat from resources

		SERVER_ACK,					///< ack for blocking communication
		SERVER_QUIT,				///< resource manager is shutdown -> quit messages to client

		DUMMY
	};

	/**
	 * use 64 bit value to avoid valgrind messages since the following
	 * union is aligned at 8 byte address
	 */
	unsigned long long package_type;

	union
	{
		struct
		{
			pid_t pid;
			unsigned long seq_id;
		} client_setup;


		struct
		{
			pid_t pid;
			unsigned long seq_id;
			double client_shutdown_hint;
		} client_shutdown;


		struct
		{
			pid_t pid;
			unsigned long seq_id;
			bool anythingChanged;	// true when something has to be changed. only if this is true, the affinity array is set up
			int number_of_cores;
			int affinity_array[1];
		}	invade_answer,			// message from server to client to update resource utilization
			reinvade_ack_async;		// message from client to server to inform server about asynchronous resource updates (field anythingChanged is ignored)


		struct
		{
			pid_t pid;
			unsigned long seq_id;

			int min_cpus, max_cpus;

			float distribution_hint;

			int scalability_graph_size;

			float scalability_graph[1];
		} invade;


		struct
		{
			pid_t pid;
			unsigned long seq_id;
		} reinvade;


		struct
		{
			pid_t pid;
			unsigned long seq_id;
		} retreat;


		struct
		{
			pid_t pid;
			unsigned long seq_id;
			double client_shutdown_hint;
			double client_shutdown_hint_div_time;
		} ack_quit;

		struct
		{
			pid_t pid;
			unsigned long seq_id;
		} ack;

		struct
		{
			unsigned long seq_id;
		} server_shutdown;
	} data;
};

#endif
