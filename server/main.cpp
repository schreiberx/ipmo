/*
 * main.cpp
 *
 *  Created on: Mar 28, 2012
 *      Author: Martin Schreiber <martin.schreiber@in.tum.de>
 */


#include "CWorldScheduler.hpp"
#include <iostream>
#include <signal.h>


CWorldScheduler *cWorldScheduler = 0;

int ctrl_c_counter = 0;
bool quit_server = false;

void myCTRLCHandler(int s)
{
	std::cout << "myCTRLCHandler" << std::endl;

	delete cWorldScheduler;
	exit(-1);

#if 0
	ctrl_c_counter++;

	if (ctrl_c_counter <= 1)
	{
		std::cout << "Press CTRL-C again to quit server" << std::endl;
		return;
	}

	quit_server = true;

	if (ctrl_c_counter <= 2)
	{
		std::cout << "Server exit activated. Press CTRL-C again to force shutdown" << std::endl;
		return;
	}

	delete cWorldScheduler;
	exit(-1);
#endif
}



int verbosity_level = 2;
int max_cores = -1;
bool color_mode = false;

int main(int argc, char *argv[])
{
	char optchar;
	while ((optchar = getopt(argc, argv, "cn:v:")) > 0)
	{
		switch(optchar)
		{
		case 'v':
			verbosity_level = atoi(optarg);
			break;

		case 'c':
			color_mode = true;
			break;

		case 'n':
			max_cores = atoi(optarg);
			break;

		case 'h':
		default:
			goto parameter_error;
		}
	}
	goto parameter_ok;

parameter_error:
	std::cout << "usage: " << argv[0] << std::endl;
	std::cout << "	[-v [int]: verbose mode (0-100), tabular output (-99)]" << std::endl;
	std::cout << "	[-n [int]: number of threads to use]" << std::endl;
	std::cout << "	[-c : activate color mode]" << std::endl;
	return -1;


parameter_ok:
	std::cout << "STARTING WORLD SCHEDULER SERVER" << std::endl;

	cWorldScheduler = new CWorldScheduler(max_cores, verbosity_level, color_mode);

//	signal(SIGABRT, &myCTRLCHandler);
//	signal(SIGTERM, &myCTRLCHandler);
	signal(SIGINT, &myCTRLCHandler);

	while (!quit_server)
	{
		if (!cWorldScheduler->action())
			break;
	}

	std::cout << "SHUTTING DOWN WORLD SCHEDULER SERVER" << std::endl;

	delete cWorldScheduler;

	return 0;
}
