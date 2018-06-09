// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "elevatortest.h"
#include "thread.h"
#include "synch.h" 

// testnum is set in main.cc
int testnum = 1;  
//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

void
SimpleThread(int which)
{
	int num;
	
	for (num = 0; num < 5; num++) 
	{
		printf("*** thread %d looped %d times\n", which, num);
		/* lab1 begin */
		PrintThreadStates();
		/* lab1 end */
		currentThread->Yield();
	}
}
//----------------------------------------------------------------------
// ThreadTest1
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest1()
{
	DEBUG('t', "Entering ThreadTest1");

	Thread *t = new Thread("forked thread");

	t->Fork(SimpleThread, (void*)1);
	SimpleThread(0);
}
/* lab1 begin */
//----------------------------------------------------------------------
// ThreadTest2
// 	New 128 threads.
//----------------------------------------------------------------------

void
ThreadTest2()
{
	DEBUG('t', "Entering ThreadTest2");
	for (int i = 0; i < 127; ++i)
	{
		Thread *t = new Thread("forked thread");
		if (i == 126)
			PrintThreadStates();
	}
}
/* lab1 end */

/* lab3 begin */
//----------------------------------------------------------------------
// RW_sem
// 	Reader & Writer (writer first)
//  semaphore implemented version
//----------------------------------------------------------------------
int wCnt = 0, rCnt = 0;
Semaphore *mutex = new Semaphore("mutex", 1);
Semaphore *bufMutex = new Semaphore("bufMutex", 1);
Semaphore *Rmutex = new Semaphore("Rmutex", 1);
char buffer[50] = "empty!";

void
ReaderSem(int num)
{    
	char readContent[50];
	
	Rmutex->P();
	Rmutex->V();

	bufMutex->P();
	sscanf(buffer, "%[^\n]", readContent);
	printf("reader %d read \"%s\"\n", num, readContent);
	bufMutex->V();

}

void
WriterSem(int num)
{    
	mutex->P();
	if (wCnt == 0)
		Rmutex->P();
	wCnt++;
	mutex->V();

	bufMutex->P();
	sprintf(buffer, "Hello from writer %d.", num);
	printf("writer %d modify\n", num);
	bufMutex->V();

	mutex->P();
	if (wCnt == 1)
		Rmutex->V();
	wCnt--;
	mutex->V();

}

void
RW_sem()
{
	DEBUG('t', "Entering RW_semaphore");
	Thread *tr[5], *tw[2];
	for (int i = 0; i < 5; ++i)
	{
		tr[i] = new Thread("reader");
		tr[i]->Fork(ReaderSem, (void*)i);
		if(i & 1)
		{
			tw[i >> 1] = new Thread("writer");
			tw[i >> 1]->Fork(WriterSem, (void*)(i >> 1));
		}
	}
	currentThread->Yield();
}
Lock *bufLock = new Lock("buffer");
Condition *rCond = new Condition("read condtion");
Condition *wCond = new Condition("write condtion");

void
ReaderCond(int num)
{    
	char readContent[50];
	while (wCnt > 0)
		rCond->Wait(bufLock);

	rCnt++;
	sscanf(buffer, "%[^\n]", readContent);
	printf("reader %d read \"%s\"\n", num, readContent);
	//wCond->Print();
	rCnt--;

	if (wCnt > 0)
		wCond->Signal(bufLock);
	else if (rCnt > 0)
		rCond->Signal(bufLock);
}

void
WriterCond(int num)
{    
	while (wCnt > 0)
		wCond->Wait(bufLock);
	
	wCnt++;
	sprintf(buffer, "Hello from writer %d.", num);
	printf("writer %d modify\n", num);
	//wCond->Print();
	wCnt--;

	if (wCnt > 0)
		wCond->Signal(bufLock);
	else if (rCnt > 0)
		rCond->Signal(bufLock);
}

void
RW_cond()
{
	DEBUG('t', "Entering RW_condition");
	Thread *tr[5], *tw[2];
	for (int i = 0; i < 5; ++i)
	{
		tr[i] = new Thread("reader");
		tr[i]->Fork(ReaderCond, (void*)i);
		if(i & 1)
		{
			tw[i >> 1] = new Thread("writer");
			tw[i >> 1]->Fork(WriterCond, (void*)(i >> 1));
		}
	}
	currentThread->Yield();
}

Barrier *br = new Barrier(4);

void
sayHi(int num)
{
	printf("Hello from thread %d\n", num);
	br->Wait();
}

void
barrier_test()
{
	Thread *t[4];
	for (int i = 0; i < 4; ++i)
	{
		t[i] = new Thread("barrier");
		t[i]->Fork(sayHi, (void*)i);
	}
	while (br->activate == true)
		currentThread->Yield();
	printf("4 threads say hi!\n");
}

RWlock *rw = new RWlock;

void readBuffer(int num)
{
	char readContent[50];
	rw->ReadLock();
	sscanf(buffer, "%[^\n]", readContent);
	printf("reader %d read \"%s\"\n", num, readContent);
	rw->Print();
	rw->ReadUnlock();
}
void writeBuffer(int num)
{
	rw->WriteLock();
	sprintf(buffer, "Hello from writer %d.", num);
	printf("writer %d modify\n", num);
	rw->Print();
	rw->WriteUnlock();
}

void rwlockTest()
{
	DEBUG('t', "Entering RW_condition");
	Thread *tr[5], *tw[2];
	for (int i = 0; i < 5; ++i)
	{
		tr[i] = new Thread("reader");
		tr[i]->Fork(readBuffer, (void*)i);
		if(i & 1)
		{
			tw[i >> 1] = new Thread("writer");
			tw[i >> 1]->Fork(writeBuffer, (void*)(i >> 1));
		}
	}
	currentThread->Yield();
}
/* lab3 end */


/* lab8 begin */
void sendm(int from)
{
	char message[30];
	sprintf(message, "Hello from sender %d", from);
	int num = currentThread->SendM(from + 2, message, 30);
	if (num < 0)
		printf("Send failed\n");
	currentThread->Yield();
}
void receivem(int to)
{
	char message[30];
	currentThread->ReceiveM(to, message, 30);
	printf("Receiver %d receive \"%s\"\n", to, message);
	currentThread->Yield();
}
void messageTest()
{
	Thread *sdr[2], *recvr[2];
	sdr[0] = new Thread("sender 1");
	sdr[1] = new Thread("sender 2");
	recvr[0] = new Thread("receiver 1");
	recvr[1] = new Thread("receiver 2");
	for (int i = 1; i <= 2; ++i)
	{
		sdr[i - 1]->Fork(sendm, (void*)i);
		recvr[i - 1]->Fork(receivem, (void*)i);
	}
	currentThread->Yield();
}
/* lab8 end */

//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------

void
ThreadTest()
{
	switch (testnum) 
	{
	case 1:
	{
		ThreadTest1();
		break;
	}
	case 2:
	{
		ThreadTest2();
		break;
	}
	case 3:
	{
		RW_sem();
		break;
	}
	case 4:
	{
		RW_cond();
		break;
	}
	case 5:
	{
		barrier_test();
		break;
	}
	case 6:
	{
		rwlockTest();
		break;
	}
	case 7:
	{
		messageTest();
		break;
	}
	default:
	printf("No test specified.\n");
	break;
	}
}

