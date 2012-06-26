/*
 * CMessageQueueServer.hpp
 *
 *  Created on: Mar 29, 2012
 *      Author: schreibm
 */

#ifndef CMESSAGE_QUEUE_SERVER_HPP
#define CMESSAGE_QUEUE_SERVER_HPP

#include <iostream>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <stdio.h>
#include <cassert>
#include <stdlib.h>
#include <errno.h>
#include <string.h>


#include "CMessageQueue.hpp"

/**
 * message queue class abstraction.
 *
 * related to example code given from the following webpage.
 *
 * http://beej.us/guide/bgipc/output/html/multipage/mq.html
 */
class CMessageQueueServer	: public CMessageQueue
{
public:
	/**
	 * setup server message queue
	 *
	 * the mtype of the server is 1 (using '0' is forbidden)
	 */
	CMessageQueueServer(
			int i_verbose_level,
			int i_msg_qbytes = -1		///< size of message queue
		)	:
		CMessageQueue(i_verbose_level)
	{
		key = ftok("/tmp/messageQueue", 'I');

		msqid = msgget(key, 0666 | IPC_CREAT);

		if (msqid == -1)
		{
			perror("Error during creation of server message queue!");
			exit(-1);
		}

		msqid_ds m;
		if (msgctl(msqid, IPC_STAT, &m) == -1)
		{
			perror("msgctl / IPC_STAT");
			exit(-1);
		}

		if (verbose_level > 2)
		{
			std::cout << "Message Queue stats:" << std::endl;
			std::cout << " + qnum (messages in queue): " << m.msg_qnum << std::endl;
			std::cout << " + qbytes (bytes allowed in queue): " << m.msg_qbytes << std::endl;
		}

		if (i_msg_qbytes > 0)
		{
			if (verbose_level > 2)
			{
				std::cout << "Updating message queue size:" << std::endl;
			}

			m.msg_qbytes = i_msg_qbytes;
			if (msgctl(msqid, IPC_SET, &m) == -1)
			{
				perror("msgctl / IPC_SET");
				exit(-1);
			}

			if (verbose_level > 2)
			{
				if (msgctl(msqid, IPC_STAT, &m) == -1)
				{
					perror("msgctl / IPC_STAT");
					exit(-1);
				}

				std::cout << "Message Queue stats:" << std::endl;
				std::cout << " + qnum (messages in queue): " << m.msg_qnum << std::endl;
				std::cout << " + qbytes (bytes allowed in queue): " << m.msg_qbytes << std::endl;
			}
		}

		/*
		 * empty message queue
		 */
		while (msgrcv(msqid, msg_buffer, max_msg_size, 1, IPC_NOWAIT) == EAGAIN);
	}



	/**
	 * send a message packed into msg_data_load_ptr
	 * \param i_length	length of message
	 * \param i_mtype	type of message
	 */
	void sendToClient(
		size_t i_length,	///< length of message without mtype!
		long int i_mtype
	)
	{
		msg_buffer->mtype = i_mtype;

		if (msgsnd(msqid, msg_buffer, i_length, 0) == -1)
		{
			perror("error during msgsnd");
			exit(-1);
		}
	}



	/**
	 * receive message of given type from client application
	 *
	 * \param i_mtype	message type to receive
	 */
	int receiveFromClient(
			long i_mtype = 1
	)
	{
#if DEBUG
		// invalidate memory area
		memset(msg_buffer, 666, max_msg_size);
#endif

		int length = msgrcv(msqid, msg_buffer, max_msg_size, i_mtype, 0);

		if (length == -1)
		{
			perror("error during msgrcv");
			exit(-1);
		}

		return length;
	}



	/**
	 * deconstructor
	 *
	 * delete message queue
	 */
	~CMessageQueueServer()
	{
		msgctl(msqid, IPC_RMID, NULL);
	}
};

#endif
