// filehdr.h
//	Data structures for managing a disk file header.
//
//	A file header describes where on disk to find the data in a file,
//	along with other information about the file (for instance, its
//	length, owner, etc.)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef FILEHDR_H
#define FILEHDR_H

#include "disk.h"
#include "bitmap.h"

/* lab5 begin */
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
/* lab5 end */

#define NumDirect ((SectorSize - 2 * sizeof(int) - 64) / sizeof(int))
#define MaxFileSize ((NumDirect - 1 + SectorSize / sizeof(int)) * SectorSize)

// The following class defines the Nachos "file header" (in UNIX terms,
// the "i-node"), describing where on disk to find all of the data in the file.
// The file header is organized as a simple table of pointers to
// data blocks.
//
// The file header data structure can be stored in memory or on disk.
// When it is on disk, it is stored in a single sector -- this means
// that we assume the size of this data structure to be the same
// as one disk sector.  Without indirect addressing, this
// limits the maximum file length to just under 4K bytes.
//
// There is no constructor; rather the file header can be initialized
// by allocating blocks for the file (if it is a new file), or by
// reading it from disk.

class FileHeader
{
  public:
	bool Allocate(BitMap *bitMap, int fileSize); // Initialize a file header,
												 //  including allocating space
												 //  on disk for the file data
	void Deallocate(BitMap *bitMap);			 // De-allocate this file's
												 //  data blocks

	void FetchFrom(int sectorNumber); // Initialize file header from disk
	void WriteBack(int sectorNumber); // Write modifications to file header
									  //  back to disk

	int ByteToSector(int offset); // Convert a byte offset into the file
								  // to the disk sector containing
								  // the byte

	int FileLength(); // Return the length of the file
					  // in bytes

	void Print(); // Print the contents of the file.
	bool ExtendTo(BitMap *freeMap, int position);
	void SetCreateTime()
	{
		time_t rawtime;
    	time(&rawtime);
		strftime(createTime, sizeof(createTime) - 1, "%F %R", localtime(&rawtime));
	}
	void SetModTime()
	{
		time_t rawtime;
    	time(&rawtime);
		strftime(lastModTime, sizeof(lastModTime) - 1, "%F %R", localtime(&rawtime));
	}
	void SetUsedTime()
	{
		time_t rawtime;
    	time(&rawtime);
		strftime(lastUsedTime, sizeof(lastUsedTime) - 1, "%F %R", localtime(&rawtime));
	}
	void SetType(char *name)
	{
		char *extBegin = strrchr(name, '.');
		if (extBegin)
		{
			strncpy(type, extBegin + 1, 3);
			type[3] = '\0';
		}
		else
			type[0] = 0;
	}
	void SetIfDir(bool isd)
	{
		is_directory = isd;
	}
	bool GetIfDir()
	{
		return is_directory;
	}
	int dataSectors[NumDirect];
	
  private:
	char lastUsedTime[18];
	char lastModTime[18];
	char createTime[18];
	char type[9];
	bool is_directory;
	int numBytes;				// Number of bytes in the file
	int numSectors;				// Number of data sectors in the file
	 // Disk sector numbers for each data
								// block in the file
};

#endif // FILEHDR_H
