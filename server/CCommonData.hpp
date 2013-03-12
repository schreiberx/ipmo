/*
 * CCommonData.hpp
 *
 *  Created on: Mar 1, 2013
 *      Author: Martin Schreiber <martin.schreiber@in.tum.de>
 */

#ifndef CCOMMONDATA_HPP_
#define CCOMMONDATA_HPP_

#include <vector>

class CCommonData
{
public:
	/**
	 * verbosity level
	 */
	int verbosity_level;


	/**
	 * hints from clients to get the information about improved efficiency
	 */
	double sum_client_shutdown_hint;
	double sum_client_shutdown_hint_div_time;


	/*
	 * sequential id for message
	 */
	unsigned long seq_id;


	/**
	 * stored start time for first client
	 */
	double start_time_first_client;


	/**
	 * colored output
	 */
	bool color_mode;


	CCommonData(
			int i_verbose_level,
			int i_color_mode
	)	:
		verbosity_level(i_verbose_level),
		sum_client_shutdown_hint(0),
		sum_client_shutdown_hint_div_time(0),
		seq_id(1),
		start_time_first_client(-1),
		color_mode(i_color_mode)
	{
	}
};


#endif /* CCOMMONDATA_HPP_ */
