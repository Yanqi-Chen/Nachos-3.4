// synch.cc 
//	Routines for synchronizing threads.  Three kinds of
//	synchronization routines are defined here: semaphores, locks 
//   	and condition variables (the implementation of the last two
//	are left to the reader).
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation.  We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts.  While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synch.h"
#include "system.h"

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	Initialize a semaphore, so that it can be used for synchronization.
//
//	"debugName" is an arbitrary name, useful for debugging.
//	"initialValue" is the initial value of the semaphore.
//----------------------------------------------------------------------

Semaphore::Semaphore(char* debugName, int initialValue)
{
	name = debugName;
	value = initialValue;
	queue = new List;
}

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	De-allocate semaphore, when no longer needed.  Assume no one
//	is still waiting on the semaphore!
//----------------------------------------------------------------------

Semaphore::~Semaphore()
{
	delete queue;
}

//----------------------------------------------------------------------
// Semaphore::P
// 	Wait until semaphore value > 0, then decrement.  Checking the
//	value and decrementing must be done atomically, so we
//	need to disable interrupts before checking the value.
//
//	Note that Thread::Sleep assumes that interrupts are disabled
//	when it is called.
//----------------------------------------------------------------------

void
Semaphore::P()
{
	IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
	
	while (value == 0) { 			// semaphore not available
	queue->Append((void *)currentThread);	// so go to sleep
	currentThread->Sleep();
	} 
	value--; 					// semaphore available, 
						// consume its value
	
	(void) interrupt->SetLevel(oldLevel);	// re-enable interrupts
}

//----------------------------------------------------------------------
// Semaphore::V
// 	Increment semaphore value, waking up a waiter if necessary.
//	As with P(), this operation must be atomic, so we need to disable
//	interrupts.  Scheduler::ReadyToRun() assumes that threads
//	are disabled when it is called.
//----------------------------------------------------------------------

void
Semaphore::V()
{
	Thread *thread;
	IntStatus oldLevel = interrupt->SetLevel(IntOff);

	thread = (Thread *)queue->Remove();
	if (thread != NULL)	   // make thread ready, consuming the V immediately
	scheduler->ReadyToRun(thread);
	value++;
	(void) interrupt->SetLevel(oldLevel);
}

void
Semaphore::Print()
{
    printf("Semaphore queue contents:\n");
    queue->Mapcar((VoidFunctionPtr) ThreadPrint);
	printf("\n\n");
}

// Dummy functions -- so we can compile our later assignments 
// Note -- without a correct implementation of Condition::Wait(), 
// the test case in the network assignment won't work!
Lock::Lock(char* debugName) 
{
	name = debugName;
	owner = NULL;
	sem = new Semaphore("lock", 1);
}

Lock::~Lock() 
{
	delete sem;
}

void 
Lock::Print()
{
	sem->Print();
}

void 
Lock::setOwner(Thread* newOwner)
{
	owner = newOwner;
}

void 
Lock::Acquire() 
{
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
	sem->P();
	setOwner(currentThread);
	(void) interrupt->SetLevel(oldLevel);
}

void 
Lock::Release() 
{
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
	setOwner(NULL);
	sem->V();
	(void) interrupt->SetLevel(oldLevel);
}

bool 
Lock::isHeldByCurrentThread()
{
	return (owner == currentThread);
}

Condition::Condition(char* debugName) 
{
	name = debugName;
	waitQueue = new List;
}

Condition::~Condition() 
{ 
	delete waitQueue;
}

void 
Condition::Wait(Lock* conditionLock) 
{
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
	conditionLock->Release();
	waitQueue->Append(currentThread);
	currentThread->Sleep();
	conditionLock->Acquire();
	(void) interrupt->SetLevel(oldLevel);
}

void 
Condition::Signal(Lock* conditionLock) 
{	
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
	Thread* asleepThread = (Thread*)(waitQueue->Remove());
	if (asleepThread != NULL)
		scheduler->ReadyToRun(asleepThread);
	(void) interrupt->SetLevel(oldLevel);
}

void 
Condition::Broadcast(Lock* conditionLock) 
{
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
	Thread* asleepThread;
	while ((asleepThread = (Thread*)(waitQueue->Remove())) != NULL)
		scheduler->ReadyToRun(asleepThread);
	(void) interrupt->SetLevel(oldLevel);
}

void
Condition::Print()
{
	printf("Conditon wait queue contents:\n");
    waitQueue->Mapcar((VoidFunctionPtr) ThreadPrint);
	printf("\n\n");
}

Barrier::Barrier(int cnt)
{
	count = cnt;
	activate = true;
	total = 0;
	cond = new Condition("barrier cond");
	mutex = new Lock("barrier lock");
}

Barrier::~Barrier()
{
	delete cond;
	delete mutex;
}

void
Barrier::Wait()
{
	mutex->Acquire();
	total++;

	// last thread reach here
	if (total == count)
	{
		cond->Broadcast(mutex);
		activate = false;
	}
	else
		cond->Wait(mutex);
	mutex->Release();
}

RWlock::RWlock()
{
	rCnt = 0;
	rwlock = new Lock("read-write lock");
	mutex = new Lock("mutex lock");
	buflock = new Lock("buffer lock");
}

RWlock::~RWlock()
{
	delete mutex;
	delete rwlock;
	delete buflock;
}

void
RWlock::ReadLock()
{
	mutex->Acquire();
	if (rCnt == 0)
		rwlock->Acquire();
	rCnt++;
	mutex->Release();

	buflock->Acquire();
}

void
RWlock::ReadUnlock()
{
	buflock->Release();

	mutex->Acquire();
	rCnt--;
	if (rCnt == 0)
		rwlock->Release();
	mutex->Release();
}

void
RWlock::WriteLock()
{
	while (rCnt > 0)
		rwlock->Acquire();

	buflock->Acquire();
}
void
RWlock::WriteUnlock()
{
	buflock->Release();

	rwlock->Release();
}