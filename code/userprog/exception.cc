// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "machine.h"
#include "system.h"
#include "syscall.h"

void SyscallEnd()
{
	int pc = machine->ReadRegister(PCReg);
	machine->WriteRegister(PrevPCReg, pc);
	pc = machine->ReadRegister(NextPCReg);
	machine->WriteRegister(PCReg, pc);
	pc += 4;
	machine->WriteRegister(NextPCReg, pc);
	return;
}

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions
//	are in machine.h.
//----------------------------------------------------------------------
//#define LRU
void ExceptionHandler(ExceptionType which)
{
	int type = machine->ReadRegister(2);
	int arg1 = machine->ReadRegister(4);
	int arg2 = machine->ReadRegister(5);
	int arg3 = machine->ReadRegister(6);

	int i;
	unsigned int vpn, offset;

	if (which == SyscallException)
	{
		switch (type)
		{
		case SC_Halt:
		{
			DEBUG('a', "Shutdown, initiated by user program.\n");
			interrupt->Halt();
			break;
		}
		case SC_Exit:
		{
			DEBUG('a', "Exit call\n");
			printf("Thread \"%s\" end with exit code %d\n", currentThread->getName(), arg1);
#ifdef USE_TLB
#ifdef LRU
			printf("LRU:\n");
#else
			printf("FIFO:\n");
#endif
			printf("miss number:%d, total number:%d\n", missCnt, memCnt);
			printf("miss rate:%f\n", float(missCnt) / memCnt);
#endif
			PrintThreadStates();
			currentThread->Finish();
			break;
		}
		case SC_Create:
		{
			int nameaddr = arg1;
			int count = 0;
			int value;
			do
			{
				machine->ReadMem(arg1, 1, &value);
				count++;
			} while (value != 0);
			printf("Filename length %d\n", count);
			arg1 = arg1 - count;
			char *name = new char[count];
			for (int i = 0; i < count; ++i)
			{
				machine->ReadMem(nameaddr + i, 1, &value);
				name[i] = (char)value;
			}
			fileSystem->Create(name, 256);
			break;
		}
		case SC_Open:
		{
			int nameaddr = arg1;
			int count = 0;
			int value;
			do
			{
				machine->ReadMem(arg1, 1, &value);
				count++;
			} while (value != 0);
			printf("Filename length %d\n", count);
			arg1 = arg1 - count;
			char *name = new char[count];
			for (int i = 0; i < count; ++i)
			{
				machine->ReadMem(nameaddr + i, 1, &value);
				name[i] = (char)value;
			}
			OpenFile *openFile = fileSystem->Open(name);
			delete[] name;
			printf("File pointer: %p\n", openFile);
			if (!openFile)
				printf("File not existed!\n");
			machine->WriteRegister(2, (int)openFile);
			break;
		}
		case SC_Close:
		{
			OpenFile *openFile = (OpenFile*)arg1;
			printf("File pointer: %p\n", openFile);
			delete openFile;
			break;
		}
		case SC_Read:
		{
			int nameaddr = arg1;
			int size = arg2;
			int fd = arg3;
			int value;
			OpenFile *openFile = (OpenFile*)fd;
			if (!openFile)
				printf("File not existed!\n")
			else
			{
				char *content = new char[size];
				openFile->Read(content, size);
				content[size] = 0;
				printf("Read \"%s\"\n", content);
				for (int i = 0; i < size; ++i)
				{
					value = (int)content[i];
					machine->WriteMem(nameaddr + i, 1, value);
				}
			}
			break;
		}
		case SC_Write:
		{
			int nameaddr = arg1;
			int size = arg2;
			int fd = arg3;
			int value;
			OpenFile *openFile = (OpenFile*)fd;
			if (!openFile)
				printf("File not existed!\n")
			else
			{
				char *content = new char[size];
				for (int i = 0; i < size; ++i)
				{
					machine->ReadMem(nameaddr + i, 1, &value);
					content[i] = (char)value;
				}
				printf("Write \"%s\"\n", content);
				openFile->Write(content, size);
			}
			break;
		}
		case SC_Exec:
		{
			DEBUG('a', "Exec call\n");

			break;
		}
		case SC_Fork:
		{
			break;
		}
		default:;
		}
		SyscallEnd();
	}
	/* lab4 begin */
	else if (which == PageFaultException)
	{
		int badVAddr = machine->registers[BadVAddrReg];
		vpn = (unsigned)badVAddr / PageSize;
		// use TLB
		if (machine->tlb != NULL)
		{
			for (i = 0; i < TLBSize; ++i)
			{
				if (machine->tlb[i].valid == false)
				{
					machine->tlb[i].valid = true;
					machine->tlb[i].interval = 0;
					if (i == 0)
						machine->tlb[0].replace = true;
					machine->tlb[i].virtualPage = machine->tlb[i].physicalPage = vpn;
					break;
				}
			}
			if (i == TLBSize)
			{
#ifdef LRU
				int LRUid, max_intv = -1;
				for (i = 0; i < TLBSize; ++i)
				{
					if (machine->tlb[i].interval > max_intv)
					{
						LRUid = i;
						max_intv = machine->tlb[i].interval;
					}
				}
				machine->tlb[LRUid].interval = 0;
				machine->tlb[LRUid].virtualPage = machine->tlb[LRUid].physicalPage = vpn;
#else
				int FIFOid;
				for (i = 0; i < TLBSize; ++i)
				{
					if (machine->tlb[i].replace)
					{
						FIFOid = i;
						machine->tlb[i].replace = false;
						machine->tlb[(i + 1) % TLBSize].replace = true;
						break;
					}
				}
				machine->tlb[FIFOid].virtualPage = machine->tlb[FIFOid].physicalPage = vpn;
#endif
			}
		}
		// use pagetable
		else
		{
			TranslationEntry *ptable = currentThread->space->pageTable;
			int ppn;
			int LRUid, max_intv = -1;
			int npages = currentThread->space->numPages;

			// Need replace(LRU)
			if ((ppn = machine->memMap->Find()) == -1)
			{
				int poffset, voffset;
				for (int i = 0; i < npages; ++i)
				{
					if (ptable[i].valid && (!ptable[i].noSwap) && (ptable[i].interval > max_intv))
					{
						max_intv = ptable[i].interval;
						LRUid = i;
					}
				}
				printf("New vpn %d replace vpn %d in ppn %d\n", vpn, LRUid, ptable[LRUid].physicalPage);
				if (ptable[LRUid].dirty)
				{
					poffset = ptable[LRUid].physicalPage * PageSize;
					voffset = ptable[LRUid].virtualPage * PageSize;
					currentThread->space->swapFile->WriteAt(&(machine->mainMemory[poffset]), PageSize, voffset);
				}
				ptable[LRUid].valid = FALSE;
				ppn = ptable[LRUid].physicalPage;
			}
			printf("Place vpn %d in ppn %d\n", vpn, ppn);
			ptable[vpn].virtualPage = vpn;
			ptable[vpn].physicalPage = ppn;
			ptable[vpn].dirty = FALSE;
			ptable[vpn].valid = TRUE;
			ptable[vpn].use = TRUE;
			ptable[vpn].interval = 0;
			currentThread->space->swapFile->ReadAt(&(machine->mainMemory[ppn * PageSize]), PageSize, vpn * PageSize);
		}
	}
	/* lab4 end */
	else
	{
		ASSERT(FALSE);
		printf("Unexpected user mode exception %d %d\n", which, type);
	}
}