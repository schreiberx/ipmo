/*
 * CMessageQueueClient.hpp
 *
 *  Created on: Apr 19, 2012
 *      Author: schreibm
 */

#ifndef CMESSAGE_QUEUE_CLIENT_HPP
#define CMESSAGE_QUEUE_CLIENT_HPP

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <iostream>
#include <stdio.h>
#include <cassert>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "CMessageQueue.hpp"



/**
 * client class
 */
class CMessageQueueClient	:
	public CMessageQueue
{

public:
	/**
	 * setup client message queue
	 *
	 * the client has to specify its send mtype which is usually its pid
	 */
	CMessageQueueClient(int i_verbose_level = 0)	:
		CMessageQueue(verbose_level)
	{
		key = ftok("/tmp/messageQueue", 'I');
		msqid = msgget(key, 0644);

		if (msqid == -1)
		{
			perror("Error during creation of client message queue!");
			std::cerr << "The server was not started most probably." << std::endl;

			assert(false);
		}
	}



	/**
	 * send a packed message to the server
	 */
	void sendToServer(
		size_t i_length,	///< length of message without mtype!
		long i_mtype = 1	///< server
	)
	{
		msg_buffer->mtype = i_mtype;

		if (msgsnd(msqid, msg_buffer, i_length, 0) == -1)
		{
			perror("error during msgsnd");
			std::cerr << "Server possibly shutdown?" << std::endl;
			exit(-1);
		}
	}



	/**
	 * receive a message from the server
	 */
	int receiveFromServer(
		long i_mtype		/// usually set to client pid for unique communication direction
	)
	{
		int len;
#if DEBUG
		// invalidate memory area
		memset(msg_buffer, -99, max_msg_size);
#endif

		len = msgrcv(msqid, msg_buffer, max_msg_size, i_mtype, 0);

		if (len == -1)
		{
			perror("error during msgrcv");
			std::cerr << "Server possibly shutdown?" << std::endl;
			exit(-1);
		}

		return len;
	}



	/**
	 * receive a message from the server
	 */
	int receiveNonblockingFromServer(
		long i_mtype		/// usually set to client pid for unique communication direction
	)
	{
		int len;
		len = msgrcv(msqid, msg_buffer, max_msg_size, i_mtype, IPC_NOWAIT);

		if (len == -1)
		{
			// return -1 if there's no message enqueued
			if (errno == ENOMSG)
				return -1;

			perror("error during msgrcv");
			std::cerr << "Server possibly shutdown?" << std::endl;
			exit(-1);
		}

		return len;
	}
};

#endif
