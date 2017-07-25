#include "osm.h"
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

const int iterations = 50000;
const int kilo = 1000;

int osm_init()
{
	return 0;
}

void emptyFunc()
{
 //empty function - does nothing (for measurement only)
}

double osm_function_time(unsigned int osm_iterations)
{
	struct timeval tvalBefore, tvalAfter;
	int i;
	//get the time of day before the iterations
	gettimeofday(&tvalBefore, NULL);
	for (i = 0; i < (int)osm_iterations / 10; i++)
	{
	//run the function 10 times to save for loop iterations.
		emptyFunc();
		emptyFunc();
		emptyFunc();
		emptyFunc();
		emptyFunc();
		emptyFunc();
		emptyFunc();
		emptyFunc();
		emptyFunc();
		emptyFunc();
	}
	//get the time day after the iterations and measure the difference
	gettimeofday(&tvalAfter, NULL);
	//if the after time is smaller than the before, then return error -1
	if ((double)tvalAfter.tv_usec < (double)tvalBefore.tv_usec)
	{
		return -1;
	}
	int totalTime = (int)((double)tvalAfter.tv_usec - (double)tvalBefore.tv_usec);
	//return the difference in nanosec
	return (double)totalTime / (((int)osm_iterations / 10) * 10) * kilo;
}

double osm_syscall_time(unsigned int osm_iterations)
{
	struct timeval tvalBefore, tvalAfter;
	int i;
	gettimeofday(&tvalBefore, NULL);
	for (i = 0; i < (int)osm_iterations / 10; i++)
	{
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
	}
	//get the time day after the iterations and measure the difference
	gettimeofday(&tvalAfter, NULL);
	//if the after time is smaller than the before, then return error -1
	if ((double)tvalAfter.tv_usec < (double)tvalBefore.tv_usec)
	{
		return -1;
	}
	int totalTime = (int)((double)tvalAfter.tv_usec - (double)tvalBefore.tv_usec);
	//return the difference in nanosec
	return (double)totalTime / (((int)osm_iterations / 10) * 10) * kilo;
}

double osm_operation_time(unsigned int osm_iterations)
{
	struct timeval tvalBefore, tvalAfter;
	int i, x;
	gettimeofday(&tvalBefore, NULL);
	for (i = 0; i < (int)osm_iterations / 10; i++)
	{
		x = i;
		x = i;
		x = i;
		x = i;
		x = i;
		x = i;
		x = i;
		x = i;
		x = i;
		x = i;
	}
	//get the time day after the iterations and measure the difference
	gettimeofday(&tvalAfter, NULL);
	//if the after time is smaller than the before, then return error -1
	if ((double)tvalAfter.tv_usec < (double)tvalBefore.tv_usec)
	{
		return -1;
	}
	x = 10;
	int totalTime = (int)((double)tvalAfter.tv_usec - (double)tvalBefore.tv_usec);
	//return the difference in nanosec
	return (double)totalTime / (((int)osm_iterations / x) * x) * kilo;
}

timeMeasurmentStructure measureTimes (unsigned int osm_iterations)
{
	timeMeasurmentStructure t;
	int x = gethostname(t.machineName, HOST_NAME_MAX);
	//if the return value of gethostname is -1, then it failed, set machine name to
	//null string
	if (x == -1)
	{
		t.machineName[0] = '\0';
	}
	//if given iteration number is 0, set to default.
	if (osm_iterations == 0)
	{
		osm_iterations = iterations;
	}
	//update all values in the struct t
	t.numberOfIterations = (osm_iterations / 10) * 10;
	t.instructionTimeNanoSecond = osm_operation_time(osm_iterations); 
	t.functionTimeNanoSecond = osm_function_time(osm_iterations); 
	t.trapTimeNanoSecond = osm_syscall_time(osm_iterations); 
	t.functionInstructionRatio = t.functionTimeNanoSecond / t.instructionTimeNanoSecond;
	t.trapInstructionRatio = t.trapTimeNanoSecond / t.instructionTimeNanoSecond;
	return t;
}
