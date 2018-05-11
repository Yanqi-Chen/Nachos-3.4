
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synchconsole.h"
#include "system.h"
#include "console.h"
#include <ctype.h>

static Semaphore *readAvail;
static Semaphore *writeDone;
static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }
static const char *escapev = "\a\b\t\n\v\f\r\0";
static const char *escapec = "abtnvfr0";

SynchConsole::SynchConsole(char *readFile, char *writeFile)
{
    readAvail = new Semaphore("read avail", 0);
    writeDone = new Semaphore("write done", 0);
    lock = new Lock("synch console lock");
    console = new Console(readFile, writeFile, ReadAvail, WriteDone, 0);
}

SynchConsole::~SynchConsole()
{
    delete console;
    delete lock;
    delete writeDone;
    delete readAvail;
}

void SynchConsole::PutChar(char ch)
{
    lock->Acquire();
    console->PutChar(ch);
    writeDone->P();
    lock->Release();
}

char SynchConsole::GetChar()
{
    lock->Acquire();
    readAvail->P();
    char ch = console->GetChar();
    if (isprint(ch))
        printf("\nGet char:'%c'\n", ch);
    else
    {
        const char *p = strchr(escapev, ch);
        if (p)
            printf("\nGet char:'\\%c'\n", escapec[p - escapev]);
    }
    lock->Release();
    return ch;
}

