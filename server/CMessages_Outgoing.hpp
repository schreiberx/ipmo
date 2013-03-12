/*
 * CMessages_Outgoing.hpp
 *
 *  Created on: Mar 1, 2013
 *      Author: Martin Schreiber <martin.schreiber@in.tum.de>
 */

#ifndef CMESSAGES_OUTGOING_HPP_
#define CMESSAGES_OUTGOING_HPP_

#include <string.h>

#include "../include/CMessageQueueServer.hpp"
#include "../include/CMessageQueueClient.hpp"

#include "CClient.hpp"
#include "CResources.hpp"
#include "CCommonData.hpp"

/**
 * world scheduler managing the clients
 */
class CMessages_Outgoing
{
public:
	CMessageQueueServer *cMessageQueueServer;

	CCommonData *cCommonData;

	/**
	 * outgoing messages
	 */
	CMessages_Outgoing()	:
		cMessageQueueServer(0),
		cCommonData(0)
	{

	}


	void setup(
			CMessageQueueServer *i_cMessageQueueServer,
			CCommonData *i_cCommonData
	)
	{
		cMessageQueueServer = i_cMessageQueueServer;
		cCommonData = i_cCommonData;
	}



	/**
	 * send outgoing ack
	 */
	void msg_outgoing_ack(
			pid_t i_client_pid		///< send ACK to client
	)
	{
		SPMOMessage &m = *(SPMOMessage*)(cMessageQueueServer->msg_data_load_ptr);

		m.package_type = SPMOMessage::SERVER_ACK;
		m.data.ack.seq_id = (cCommonData->seq_id)++;

		cMessageQueueServer->sendToClient(
				(size_t)&(m.data) - (size_t)&m +
				sizeof(m.data.ack),
				i_client_pid
			);
	}




	/**
	 * send shutdown ack
	 */
	void msg_outgoing_shutdown_ack(
			pid_t i_client_pid
	)
	{
		SPMOMessage &m = *(SPMOMessage*)(cMessageQueueServer->msg_data_load_ptr);

		m.package_type = SPMOMessage::CLIENT_ACK_SHUTDOWN;
		m.data.ack_quit.client_shutdown_hint = cCommonData->sum_client_shutdown_hint;
		m.data.ack_quit.client_shutdown_hint_div_time = cCommonData->sum_client_shutdown_hint_div_time;
		m.data.ack_quit.seq_id = cCommonData->seq_id++;

		cMessageQueueServer->sendToClient(
				(size_t)&(m.data) - (size_t)&m +
				sizeof(m.data.ack_quit),
			i_client_pid);
	}




	/**
	 * send invade answer
	 */
	void msg_outgoing_sendInvadeAnswer(
			CClient *i_cClient,			///< client to send invade answer to
			bool i_anythingChanged		///< anything changed? if not, send with 'nothing changed' flags
	)
	{
		SPMOMessage &m = *(SPMOMessage*)(cMessageQueueServer->msg_data_load_ptr);

		m.package_type = SPMOMessage::SERVER_INVADE_ANSWER;
		m.data.invade_answer.pid = i_cClient->pid;
		m.data.invade_answer.anythingChanged = i_anythingChanged;
		m.data.invade_answer.number_of_cores = i_cClient->number_of_assigned_cores;

//		std::cout << "OUTGOING MESSAGE TO CLIENT " << i_cClient->client_id << std::endl;

		if (i_anythingChanged)
		{
			int i = 0;
			for (std::list<int>::iterator iter = i_cClient->assigned_cores.begin(); iter != i_cClient->assigned_cores.end(); iter++)
			{
				m.data.invade_answer.affinity_array[i] = *iter;
				i++;
			}

			m.data.invade_answer.seq_id = cCommonData->seq_id++;

			cMessageQueueServer->sendToClient(
					(size_t)&(m.data) - (size_t)&m +
					sizeof(m.data.invade_answer)+
					sizeof(int)*(i_cClient->number_of_assigned_cores-1),
					i_cClient->pid);
		}
		else
		{
			m.data.invade_answer.seq_id = cCommonData->seq_id++;

			cMessageQueueServer->sendToClient(
					(size_t)&(m.data) - (size_t)&m +
					sizeof(m.data.invade_answer),
					i_cClient->pid);
		}
	}


};

#endif /* CWORLDSCHEDULER_HPP_ */
