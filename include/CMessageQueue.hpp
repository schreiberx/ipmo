/*
 * CMessageQueue.hpp
 *
 *  Created on: Mar 29, 2012
 *      Author: schreibm
 */

#ifndef CMESSAGE_QUEUE_HPP
#define CMESSAGE_QUEUE_HPP

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <iostream>
#include <stdio.h>
#include <cassert>
#include <stdlib.h>
#include <errno.h>
#include <string.h>



/**
 * message queue class abstraction.
 *
 * related to example code given from the following webpage.
 *
 * http://beej.us/guide/bgipc/output/html/multipage/mq.html
 */
class CMessageQueue
{
public:
	int key;	///< message queue identifier
	int msqid;	///< identifier for message queue associated to key


	/**
	 * verbose_level
	 */
	int verbose_level;


	/**
	 * maximum size of raw message (without mtype)
	 */
	size_t max_msg_size;


	/**
	 * pointer to message data
	 */
	void *msg_data_load_ptr;


	/**
	 * pointer to data allocated and referred to msg_data_ptr to access
	 * it via s_msgbuf structure information
	 */
	msgbuf *msg_buffer;


	CMessageQueue(
		int i_verbose_level = 0,
		size_t i_max_msg_size = 1024*1024
	)	:
		verbose_level(i_verbose_level),
		max_msg_size(i_max_msg_size)
	{
		msg_buffer = (msgbuf*)new char[max_msg_size+sizeof(long int)];
#if 1
		// initialize message queue with arbitrary data
		memset(msg_buffer, 255, max_msg_size+sizeof(long int));
#endif

		msg_data_load_ptr = msg_buffer->mtext;
	}



	~CMessageQueue()
	{
		delete[] (char*)msg_buffer;
	}
};

#endif
