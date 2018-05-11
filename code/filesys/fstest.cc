// fstest.cc
//	Simple test routines for the file system.
//
//	We implement:
//	   Copy -- copy a file from UNIX to Nachos
//	   Print -- cat the contents of a Nachos file
//	   Perftest -- a stress test for the Nachos file system
//		read and write a really large file in tiny chunks
//		(won't work on baseline system!)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "utility.h"
#include "filesys.h"
#include "system.h"
#include "thread.h"
#include "disk.h"
#include "stats.h"
#include "synch.h"

#define TransferSize 10 // make it small, just to be difficult

//----------------------------------------------------------------------
// Copy
// 	Copy the contents of the UNIX file "from" to the Nachos file "to"
//----------------------------------------------------------------------

void Copy(char *from, char *to)
{
    FILE *fp;
    OpenFile *openFile;
    int amountRead, fileLength;
    char *buffer;

    // Open UNIX file
    if ((fp = fopen(from, "r")) == NULL)
    {
        printf("Copy: couldn't open input file %s\n", from);
        return;
    }

    // Figure out length of UNIX file
    fseek(fp, 0, 2);
    fileLength = ftell(fp);
    fseek(fp, 0, 0);

    // Create a Nachos file of the same length
    DEBUG('f', "Copying file %s, size %d, to file %s\n", from, fileLength, to);
    if (!fileSystem->Create(to, fileLength))
    { // Create Nachos file
        printf("Copy: couldn't create output file %s\n", to);
        fclose(fp);
        return;
    }

    openFile = fileSystem->Open(to);
    ASSERT(openFile != NULL);

    // Copy the data in TransferSize chunks
    buffer = new char[TransferSize];
    while ((amountRead = fread(buffer, sizeof(char), TransferSize, fp)) > 0)
        openFile->Write(buffer, amountRead);
    delete[] buffer;

    // Close the UNIX and the Nachos files
    delete openFile;
    fclose(fp);
}

//----------------------------------------------------------------------
// Print
// 	Print the contents of the Nachos file "name".
//----------------------------------------------------------------------

void Print(char *name)
{
    OpenFile *openFile;
    int i, amountRead;
    char *buffer;

    if ((openFile = fileSystem->Open(name)) == NULL)
    {
        printf("Print: unable to open file %s\n", name);
        return;
    }

    buffer = new char[TransferSize];
    while ((amountRead = openFile->Read(buffer, TransferSize)) > 0)
        for (i = 0; i < amountRead; i++)
            printf("%c", buffer[i]);
    delete[] buffer;

    delete openFile; // close the Nachos file
    return;
}

//----------------------------------------------------------------------
// PerformanceTest
// 	Stress the Nachos file system by creating a large file, writing
//	it out a bit at a time, reading it back a bit at a time, and then
//	deleting the file.
//
//	Implemented as three separate routines:
//	  FileWrite -- write the file
//	  FileRead -- read the file
//	  PerformanceTest -- overall control, and print out performance #'s
//----------------------------------------------------------------------

#define FileName "TestFile"
#define Contents "aaaaa"
#define Contents2 "bbbbb"
#define Contents3 "ccccc"
#define ContentSize strlen(Contents)
#define FileSize ((int)(ContentSize * 5))
static Barrier barrier(6);
static void
FileWrite(int dummy)
{
    OpenFile *openFile;
    int i, numBytes;

    printf("Thread %d \"%s\" begin sequential write of %d byte file, in %d byte chunks\n", currentThread->getTid(), currentThread->getName(), FileSize, ContentSize);

    openFile = fileSystem->Open(FileName);
    if (dummy)
    {
        barrier.Wait();
    }

    if (openFile == NULL)
    {
        printf("Perf test: thread %d \"%s\" unable to open %s\n", currentThread->getTid(), FileName);
        return;
    }
    for (i = 0; i < FileSize; i += ContentSize)
    {
        if (dummy == 1)
        {
            numBytes = openFile->Write(Contents2, ContentSize);
            printf("Thread %d \"%s\" write from %d to %d bytes:\n%s\n", currentThread->getTid(), currentThread->getName(), i, i + numBytes, Contents2);
        }
        else if (dummy == 2)
        {
            numBytes = openFile->Write(Contents3, ContentSize);
            printf("Thread %d \"%s\" write from %d to %d bytes:\n%s\n", currentThread->getTid(), currentThread->getName(), i, i + numBytes, Contents3);
        }
        else
        {
            numBytes = openFile->Write(Contents, ContentSize);
            printf("Thread %d \"%s\" write from %d to %d bytes\n", currentThread->getTid(), currentThread->getName(), i, i + numBytes);
        }
        if (numBytes < ContentSize)
        {
            printf("Perf test: thread %d \"%s\" unable to write %s\n", currentThread->getTid(), currentThread->getName(), FileName);
            delete openFile;
            return;
        }
    }

    if (dummy)
    {
        //barrier.Wait();
        delete openFile; // close file
        return;
    }
    delete openFile; // close file
}

static void
FileRead(int dummy)
{
    OpenFile *openFile;
    char *buffer = new char[ContentSize + 1];
    int i, numBytes;

    printf("Thread %d \"%s\" begin sequential read of %d byte file, in %d byte chunks\n", currentThread->getTid(), currentThread->getName(), FileSize, ContentSize);

    openFile = fileSystem->Open(FileName);
    barrier.Wait();

    if (openFile == NULL)
    {
        printf("Perf test: thread %d \"%s\" unable to open file %s\n", currentThread->getTid(), currentThread->getName(), FileName);
        delete[] buffer;
        return;
    }

    for (i = 0; i < FileSize; i += ContentSize)
    {
        numBytes = openFile->Read(buffer, ContentSize);
        printf("Thread %d \"%s\" read from %d to %d bytes in round %d:\n", currentThread->getTid(), currentThread->getName(), i, i + numBytes, i / ContentSize);
        buffer[ContentSize] = 0;
        printf("%s\n", buffer);
        if (numBytes < ContentSize)
        {
            printf("Perf test: thread %d \"%s\" unable to read %s\n", currentThread->getTid(), currentThread->getName(), FileName);
            delete openFile;
            delete[] buffer;
            return;
        }
    }
    delete[] buffer;
    //barrier.Wait();
    delete openFile; // close file
}

static void
FilePrint(int dummy)
{
    OpenFile *openFile;
    openFile = fileSystem->Open(FileName);
    barrier.Wait();

    if (openFile == NULL)
    {
        printf("Perf test: unable to open file %s\n", FileName);
        return;
    }
    //barrier.Wait();
    openFile->Print();
    delete openFile;
}

void PerformanceTest()
{
    printf("Starting file system performance test:\n");
    if (!fileSystem->Create(FileName, 0))
    {
        printf("Perf test: can't create %s\n", FileName);
        return;
    }
    FileWrite(0);
    Thread *tw1 = new Thread("writer 1");
    Thread *tw2 = new Thread("writer 2");
    Thread *tr1 = new Thread("reader 1");
    Thread *tr2 = new Thread("reader 2");
    Thread *tp = new Thread("print");
    tw1->Fork(FileWrite, (void *)1);
    tw2->Fork(FileWrite, (void *)2);
    tr1->Fork(FileRead, (void *)3);
    tr2->Fork(FileRead, (void *)4);
    tp->Fork(FilePrint, (void *)5);
    barrier.Wait();
    if (!fileSystem->Remove(FileName))
    {
        printf("Perf test: unable to remove %s\n", FileName);
        return;
    }
}
