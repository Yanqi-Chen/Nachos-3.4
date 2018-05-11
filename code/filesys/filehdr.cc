// filehdr.cc
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector,
//
//      Unlike in a real system, we do not keep track of file permissions,
//	ownership, last modification date, etc., in the file header.
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "filehdr.h"
#include <ctype.h>
#include <string.h>

static const char *escapev = "\a\b\t\n\v\f\r\0";
static const char *escapec = "abtnvfr0";

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

// The size of file should be less than (13 + 32) * 128 = 5632 Bytes
// File larger than 13 * 128 = 1664 Bytes need indirect index
bool FileHeader::Allocate(BitMap *freeMap, int fileSize)
{
    numBytes = fileSize;
    numSectors = divRoundUp(fileSize, SectorSize);
    if (freeMap->NumClear() < numSectors)
        return FALSE; // not enough space
    if (fileSize > MaxFileSize)
        return FALSE;

    if (numSectors < NumDirect)
    {
        for (int i = 0; i < numSectors; i++)
            dataSectors[i] = freeMap->Find();
        dataSectors[NumDirect - 1] = -1;
    }
    else
    {
        for (int i = 0; i < NumDirect - 1; i++)
            dataSectors[i] = freeMap->Find();
        int indSec = dataSectors[NumDirect - 1] = freeMap->Find();
        int *indirectSec = new int[SectorSize / sizeof(int)];
        for (int i = 0; i < numSectors - NumDirect + 1; i++)
        {
            indirectSec[i] = freeMap->Find();
        }
        synchDisk->WriteSector(indSec, (char *)indirectSec);
        delete[] indirectSec;
    }
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void FileHeader::Deallocate(BitMap *freeMap)
{
    if (numSectors < NumDirect)
    {
        for (int i = 0; i < numSectors; i++)
        {
            ASSERT(freeMap->Test((int)dataSectors[i])); // ought to be marked!
            freeMap->Clear((int)dataSectors[i]);
        }
    }
    else
    {
        for (int i = 0; i < NumDirect - 1; i++)
        {
            ASSERT(freeMap->Test((int)dataSectors[i])); // ought to be marked!
            freeMap->Clear((int)dataSectors[i]);
        }

        int indSec = dataSectors[NumDirect - 1];         // sector number for indirect index
        ASSERT(freeMap->Test((int)indSec)); // ought to be marked!

        int *indirectSec = new int[SectorSize / sizeof(int)];
        synchDisk->ReadSector(indSec, (char *)indirectSec);
        for (int i = 0; i < numSectors - NumDirect + 1; i++)
        {
            ASSERT(freeMap->Test((int)indirectSec[i])); // ought to be marked!
            freeMap->Clear((int)indirectSec[i]);
        }

        freeMap->Clear((int)indSec);

        delete[] indirectSec;
    }
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk.
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void FileHeader::FetchFrom(int sector)
{
    synchDisk->ReadSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk.
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void FileHeader::WriteBack(int sector)
{
    synchDisk->WriteSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int FileHeader::ByteToSector(int offset)
{
    int id = offset / SectorSize;
    int indSec = dataSectors[NumDirect - 1];
    int *indirectSec = new int[SectorSize / sizeof(int)];
    if (id < NumDirect - 1)
        return (dataSectors[id]);
    ASSERT(indSec != -1);
    id -= NumDirect - 1;
    synchDisk->ReadSector(indSec, (char *)indirectSec);
    int temp = indirectSec[id];
    delete[] indirectSec;
    return temp;
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void FileHeader::Print()
{
    int i, j, k;
    char *data = new char[SectorSize];
    int indSec;
    int *indirectSec = new int[SectorSize / sizeof(int)];

    printf("\nFileHeader contents.  File size: %d.  File blocks:\n", numBytes);
    if (numSectors < NumDirect)
    {
        for (i = 0; i < numSectors; i++)
            printf("%d ", dataSectors[i]);
    }
    else
    {
        for (i = 0; i < NumDirect - 1; i++)
            printf("%d ", dataSectors[i]);

        printf("\nDirect index block: ");
        indSec = dataSectors[NumDirect - 1];
        printf("%d", indSec);

        printf("\nExtend blocks:\n");
        synchDisk->ReadSector(indSec, (char *)indirectSec);
        for (i = 0; i < numSectors - NumDirect + 1; i++)
            printf("%d ", indirectSec[i]);
    }
    putchar('\n');
    printf("Type: %s\n", type);
    printf("Create Time: %s\n", createTime);
    printf("Last Used Time: %s\n", lastUsedTime);
    printf("Last Modified Time: %s\n", lastModTime);
    printf("File contents:\n");
    for (i = k = 0; i < numSectors; i++)
    {
        if (i < NumDirect - 1)
            synchDisk->ReadSector(dataSectors[i], data);
        else
            synchDisk->ReadSector(indirectSec[i - NumDirect + 1], data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++)
        {
            if (isprint(data[j]))
                printf("%c", data[j]);
            else 
            {
                const char *p = strchr(escapev, data[j]);
                if (p)
                    printf("\\%c", escapec[p - escapev]);
            }
                
        }
        printf("\n");
    }
    putchar('\n');
    delete[] data;
    delete[] indirectSec;
}

//----------------------------------------------------------------------
// FileHeader::Extend
// 	Extend file length
//----------------------------------------------------------------------

bool FileHeader::ExtendTo(BitMap *freeMap, int position)
{
    int newNumSectors = divRoundUp(position, SectorSize);

    if (newNumSectors == numSectors)
    {
        numBytes = position;
        return true;
    }

    if (newNumSectors - numSectors > freeMap->NumClear())
        return false;
    if (position > MaxFileSize)
        return false;


    if (numSectors < NumDirect)
    {
        if (newNumSectors < NumDirect)
        {
            for (int i = numSectors; i < newNumSectors; ++i)
            {
                dataSectors[i] = freeMap->Find();
                printf("Sector %d allocated\n", dataSectors[i]);
            }
        }
        else
        {
            for (int i = numSectors; i < NumDirect - 1; ++i)
            {
                dataSectors[i] = freeMap->Find();
                printf("Sector %d allocated\n", i);
            }
            int indSec = dataSectors[NumDirect - 1] = freeMap->Find();
            printf("Extend sector %d allocated\n", indSec);
            int *indirectSec = new int[SectorSize / sizeof(int)];
            for (int i = 0; i < newNumSectors - NumDirect + 1; i++)
            {
                indirectSec[i] = freeMap->Find();
                printf("Indexed sector %d allocated\n", indirectSec[i]);
            }
            synchDisk->WriteSector(indSec, (char *)indirectSec);
            delete[] indirectSec;
        }
    }
    else
    {
        int indSec = dataSectors[NumDirect - 1];
        int *indirectSec = new int[SectorSize / sizeof(int)];
        synchDisk->ReadSector(indSec, (char *)indirectSec);
        for (int i = numSectors - NumDirect + 1; i < newNumSectors - NumDirect + 1; i++)
        {
            indirectSec[i] = freeMap->Find();
        }
        synchDisk->WriteSector(indSec, (char *)indirectSec);
        delete[] indirectSec;
    }
    numBytes = position;
    numSectors = newNumSectors;
    return true;
}
