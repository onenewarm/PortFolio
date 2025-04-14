#include <iostream>
#include "Test.h"


using namespace std;

#pragma comment(lib, "Winmm.lib")

int main()
{
	timeBeginPeriod(1);
	unsigned int threadId;

	// Event √ ±‚»≠
	for (int cnt = 0; cnt < COUNT_OF_THREADS; ++cnt)
	{
		s_ThreadEvents[cnt] = CreateEvent(NULL, true, false, NULL);
		s_ThreadParams[cnt].completeHandle = s_ThreadEvents[cnt];
	}

	
	s_StartEvent = CreateEvent(NULL, true, false, NULL);
	TestFunc<TestData<512>, 100>();
	TestFunc<TestData<512>, 200>();
	TestFunc<TestData<512>, 300>();
	TestFunc<TestData<512>, 400>();
	TestFunc<TestData<512>, 500>();
	TestFunc<TestData<512>, 600>();
	TestFunc<TestData<512>, 700>();
	TestFunc<TestData<512>, 800>();
	TestFunc<TestData<512>, 900>();
	TestFunc<TestData<512>, 1000>();;
	TestFunc<TestData<512>, 2000>();
	TestFunc<TestData<512>, 3000>();
	TestFunc<TestData<512>, 4000>();
	TestFunc<TestData<512>, 5000>();;
	TestFunc<TestData<512>, 6000>();;
	TestFunc<TestData<512>, 7000>();
	TestFunc<TestData<512>, 8000>();
	TestFunc<TestData<512>, 9000>();
	TestFunc<TestData<512>, 10000>();
	TestFunc<TestData<512>, 20000>();
	TestFunc<TestData<512>, 30000>();



	//TestFunc<TestData<512>, 800>();
	
	//TestFunc<TestData<512>, 800>(2);
	//TestFunc<TestData<512>, 800>(4);
	//TestFunc<TestData<512>, 800>(8);
	//TestFunc<TestData<512>, 800>(16);
	//TestFunc<TestData<512>, 800>(32);
	
	//TestFunc<TestData<512>, 800>(64);

	OutToCSV();

	system("python dataCSVToGraph.py");
}