/*
 * main.cpp
 *
 *  Created on: Mar 29, 2012
 *      Author: Martin Schreiber <martin.schreiber@in.tum.de>
 */


#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <errno.h>
#include <pthread.h>
#include <cassert>
#include <signal.h>
#include <vector>


#include "../include/CPMO_OMP.hpp"
#include "../include/CDummyWorkload.hpp"

CPMO_OMP *cPmo = 0;

/**
 * testrun 1:
 */
void run1(int max_threads = -1, int workload = 40000)
{
	std::cout << "RUN 1 (invade/-)" << std::endl;

	double *a = new double[cPmo->getMaxNumberOfThreads()];

	std::vector<float> v1(20, 0);
	float s = 1.0;

	std::cout << "Scalability graph: ";
	for (int i = 0; i < 20; i++)
	{
		v1[i] = s;
		s += 0.1+0.2*4.0/((double)((i+2)*(i+3)));

		std::cout << v1[i] << " ";
	}
	std::cout << std::endl;

	cPmo->invade(1, 2, v1);

	#pragma omp parallel
	{
		int thread_num = omp_get_thread_num();
		a[thread_num] = CDummyWorkload::doSomeSqrt(918238123.0, workload);
	}

	for (int i = 0; i < cPmo->getNumberOfThreads(); i++)
		std::cout << "RESULT (" << i << "): " << a[i] << std::endl;

//	cPmo->retreat();

	cPmo->client_shutdown_hint = workload;
}



void run2(int max_threads = -1, int workload = 40000)
{
	std::cout << "RUN 2 (invade/-)" << std::endl;

	double *a = new double[cPmo->getMaxNumberOfThreads()];

	std::vector<float> v1(20, 0);
	float s = 1.0;

	for (int i = 0; i < 20; i++)
	{
		v1[i] = s;
		s += 0.9+0.1*4.0/((double)((i+2)*(i+2)));
	}

	cPmo->invade(1, 1024, v1);

	#pragma omp parallel
	{
		int thread_num = omp_get_thread_num();
		a[thread_num] = CDummyWorkload::doSomeSqrt(918238123.0, workload);
	}

	for (int i = 0; i < cPmo->getNumberOfThreads(); i++)
		std::cout << "RESULT (" << i << "): " << a[i] << std::endl;

//	cPmo->retreat();

	cPmo->client_shutdown_hint = workload;
}



void run3(int max_threads = -1, int workload = 40000)
{
	std::cout << "RUN 3 (invade/retreat)" << std::endl;

	double *a = new double[cPmo->getMaxNumberOfThreads()];

	std::vector<float> v1(20, 0);
	float s = 1.0;

	for (int i = 0; i < 20; i++)
	{
		v1[i] = s;
		s += 0.1+0.9*9.0/((double)((i+2)*(i+3)));
	}

	cPmo->invade(1, 1024, v1);

	#pragma omp parallel
	{
		int thread_num = omp_get_thread_num();
		a[thread_num] = CDummyWorkload::doSomeSqrt(918238123.0, workload);
	}

	for (int i = 0; i < cPmo->getNumberOfThreads(); i++)
		std::cout << "RESULT (" << i << "): " << a[i] << std::endl;

	cPmo->retreat();

	cPmo->client_shutdown_hint = workload;
}



void run4(int max_threads = -1, int workload = 40000)
{
	std::cout << "RUN 4 (invade/retreat)" << std::endl;

	double *a = new double[cPmo->getMaxNumberOfThreads()];

	std::vector<float> v1(20, 0);
	float s = 1.0;

	for (int i = 0; i < 20; i++)
	{
		v1[i] = s;
		s += 0.9+0.1*6.0/((double)((i+2)*(i+3)));
	}

	cPmo->invade(1, 4, v1);

	#pragma omp parallel
	{
		int thread_num = omp_get_thread_num();
		a[thread_num] = CDummyWorkload::doSomeSqrt(918238123.0, workload);
	}

	for (int i = 0; i < cPmo->getNumberOfThreads(); i++)
		std::cout << "RESULT (" << i << "): " << a[i] << std::endl;

	cPmo->retreat();

	cPmo->client_shutdown_hint = workload;
}



void run5_setup(int max_threads = -1, int workload = 40000)
{
	std::cout << "RUN 5: setup" << std::endl;


	std::vector<float> v1(20, 0);
	float s = 1.0;

	for (int i = 0; i < 20; i++)
	{
		v1[i] = s;
		s += 0.9+0.1*6.0/((double)((i+2)*(i+3)));
	}

	cPmo->invade(1, 1024, v1);
}


void run5_loop(int max_threads = -1, int workload = 40000)
{
	std::cout << "RUN 5 (reinvade)" << std::endl;

	cPmo->reinvade_blocking();

	double *a = new double[cPmo->getMaxNumberOfThreads()];

	#pragma omp parallel
	{
		int thread_num = omp_get_thread_num();
		a[thread_num] = CDummyWorkload::doSomeSqrt(918238123.0, workload);
	}

	for (int i = 0; i < cPmo->getNumberOfThreads(); i++)
		std::cout << "RESULT (" << i << "): " << a[i] << std::endl;
}

void run5_shutdown(int max_threads = -1, int workload = 40000)
{
	std::cout << "RUN 5: shutdown" << std::endl;

	cPmo->client_shutdown_hint = workload;
}




void run6_setup(
		int max_threads,
		int workload
)
{
	std::cout << "RUN 6: setup" << std::endl;

	std::vector<float> v1(20, 0);
	float s = 1.0;

	for (int i = 0; i < 20; i++)
	{
		v1[i] = s;
		s += 0.9+0.1*6.0/((double)((i+2)*(i+3)));
	}

	cPmo->invade(1, 1024, v1);
}


void run6_loop(
		int max_threads,
		int workload,
		bool non_blocking_invade = false
)
{
	std::cout << "RUN 6 (reinvade nonblocking)" << std::endl;

	cPmo->reinvade_nonblocking();

	double *a = new double[cPmo->getMaxNumberOfThreads()];

	#pragma omp parallel
	{
		int thread_num = omp_get_thread_num();
		a[thread_num] = CDummyWorkload::doSomeSqrt(918238123.0, workload);
	}

	for (int i = 0; i < cPmo->getNumberOfThreads(); i++)
		std::cout << "RESULT (" << i << "): " << a[i] << std::endl;

	if (non_blocking_invade)
	{

		float v1[1024];
		float s = 1.0;

		for (int i = 0; i < 20; i++)
		{
			v1[i] = s;
			s += 0.9+0.1*6.0/((double)((i+2)*(i+3)));
		}

		cPmo->invade_nonblocking(1, 1024, 20, v1);
	}
}

void run6_shutdown(
		int max_threads,
		int workload
)
{
	std::cout << "RUN 6: shutdown" << std::endl;

	cPmo->client_shutdown_hint = workload;
}






int main(int argc, char *argv[])
{
	/*
	 * invade(
	 * 		CAnd(	CPEConstraint(3),
	 * 				CScalability(123)
	 * 			)
	 * 		)
	 */

	int max_threads = -1;
	int test_program = 1;

	if (argc > 1)
	{
		test_program = atoi(argv[1]);
	}


	cPmo = new CPMO_OMP(max_threads);
	cPmo->setup();


	switch(test_program)
	{
	case 1:
		run1(max_threads);
		break;

	case 2:
		run2(max_threads);
		break;

	case 3:
		run3(max_threads);
		break;

	case 4:
		run4(max_threads);
		break;

	case 5:
		run5_setup(max_threads);
		run5_loop(max_threads);
		run5_shutdown(max_threads);
		break;

	case 6:
		run6_setup(max_threads, 40000);
		run6_loop(max_threads, 40000);
		run6_shutdown(max_threads, 40000);
		break;


	case 7:
		run6_setup(max_threads, 40000);
		run6_loop(max_threads, 40000, true);
		run6_shutdown(max_threads, 40000);
		break;



	case 11:
		while (true)
			run1(max_threads, 4000);
		break;

	case 12:
		while (true)
			run2(max_threads, 4000);
		break;

	case 13:
		while (true)
			run3(max_threads, 4000);
		break;

	case 14:
		while (true)
			run4(max_threads, 4000);
		break;

	case 15:
		run5_setup(max_threads, 4000);
		while (true)
		{
			run5_loop(max_threads, 4000);
		}
		run5_shutdown(max_threads, 4000);
		break;

	case 16:
		run6_setup(max_threads, 40000);
		while (true)
		{
			run6_loop(max_threads, 40000);
		}
		run6_shutdown(max_threads, 40000);
		break;


	case 17:
		run6_setup(max_threads, 40000);
		while (true)
			run6_loop(max_threads, 40000, true);

		run6_shutdown(max_threads, 40000);
		break;



	case 21:
		for (int i = 0; i < 100; i++)
			run1(max_threads, 4000);
		cPmo->client_shutdown_hint = 4000*20;
		break;

	case 22:
		for (int i = 0; i < 100; i++)
			run2(max_threads, 4000);
		cPmo->client_shutdown_hint = 4000*20;
		break;

	case 23:
		for (int i = 0; i < 100; i++)
			run3(max_threads, 4000);
		cPmo->client_shutdown_hint = 4000*20;
		break;

	case 24:
		for (int i = 0; i < 100; i++)
			run4(max_threads, 4000);
		cPmo->client_shutdown_hint = 4000*20;
		break;

	case 25:
		run5_setup(max_threads, 4000);

		for (int i = 0; i < 100; i++)
			run5_loop(max_threads, 4000);

		run5_shutdown(max_threads, 4000);
		cPmo->client_shutdown_hint = 4000*20;
		break;

	case 26:
		run6_setup(max_threads, 40000);

		for (int i = 0; i < 100; i++)
			run6_loop(max_threads, 4000);

		run6_shutdown(max_threads, 4000);
		cPmo->client_shutdown_hint = 4000*20;
		break;

	case 27:
		run6_setup(max_threads, 40000);

		for (int i = 0; i < 100; i++)
			run6_loop(max_threads, 4000, true);

		run6_shutdown(max_threads, 4000);
		cPmo->client_shutdown_hint = 4000*20;
		break;
	}

    cPmo->retreat();

	delete cPmo;

	return EXIT_SUCCESS;
}
