// directory.cc
//	Routines to manage a directory of file names.
//
//	The directory is a table of fixed length entries; each
//	entry represents a single file, and contains the file name,
//	and the location of the file header on disk.  The fixed size
//	of each directory entry means that we have the restriction
//	of a fixed maximum size for file names.
//
//	The constructor initializes an empty directory of a certain size;
//	we use ReadFrom/WriteBack to fetch the contents of the directory
//	from disk, and to write back any modifications back to disk.
//
//	Also, this implementation has the restriction that the size
//	of the directory cannot expand.  In other words, once all the
//	entries in the directory are used, no more files can be created.
//	Fixing this is one of the parts to the assignment.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "utility.h"
#include "filehdr.h"
#include "directory.h"
#include "system.h"

//----------------------------------------------------------------------
// Directory::Directory
// 	Initialize a directory; initially, the directory is completely
//	empty.  If the disk is being formatted, an empty directory
//	is all we need, but otherwise, we need to call FetchFrom in order
//	to initialize it from disk.
//
//	"size" is the number of entries in the directory
//----------------------------------------------------------------------

Directory::Directory(int size)
{
    table = new DirectoryEntry[size];
    tableSize = size;
    for (int i = 0; i < tableSize; i++)
        table[i].inUse = FALSE;
}

//----------------------------------------------------------------------
// Directory::~Directory
// 	De-allocate directory data structure.
//----------------------------------------------------------------------

Directory::~Directory()
{
    delete[] table;
}

//----------------------------------------------------------------------
// Directory::FetchFrom
// 	Read the contents of the directory from disk.
//
//	"file" -- file containing the directory contents
//----------------------------------------------------------------------

void Directory::FetchFrom(OpenFile *file)
{
    (void)file->ReadAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::WriteBack
// 	Write any modifications to the directory back to disk
//
//	"file" -- file to contain the new directory contents
//----------------------------------------------------------------------

void Directory::WriteBack(OpenFile *file)
{
    (void)file->WriteAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::FindIndex
// 	Look up file name in directory, and return its location in the table of
//	directory entries.  Return -1 if the name isn't in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int Directory::FindIndex(char *name)
{
    char longName[129];
    if (strlen(name) <= FileNameMaxLen)
    {
        for (int i = 0; i < tableSize; i++)
            if (table[i].inUse && !strncmp(table[i].name, name, FileNameMaxLen))
                return i;
    }
    else
    {
        for (int i = 0; i < tableSize; i++)
            if (table[i].inUse && !table[i].short_name)
            {
                synchDisk->ReadSector(table[i].nameSector, longName);
                if (!strcmp(name, longName));
                    return i;
            }
    }
    return -1; // name not in directory
}

//----------------------------------------------------------------------
// Directory::Find
// 	Look up file name in directory, and return the disk sector number
//	where the file's header is stored. Return -1 if the name isn't
//	in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int Directory::Find(char *name)
{
    int i = FindIndex(name);

    if (i != -1)
        return table[i].sector;
    return -1;
}

//----------------------------------------------------------------------
// Directory::Add
// 	Add a file into the directory.  Return TRUE if successful;
//	return FALSE if the file name is already in the directory, or if
//	the directory is completely full, and has no more space for
//	additional file names.
//
//	"name" -- the name of the file being added
//	"newSector" -- the disk sector containing the added file's header
//----------------------------------------------------------------------

bool Directory::Add(BitMap *freeMap, char *name, int newSector)
{
    if (FindIndex(name) != -1)
        return FALSE;

    ASSERT(strlen(name) < 128);

    for (int i = 0; i < tableSize; i++)
        if (!table[i].inUse)
        {
            table[i].inUse = TRUE;
            table[i].short_name = TRUE;
            strncpy(table[i].name, name, FileNameMaxLen);
            if (strlen(name) > FileNameMaxLen)
            {
                if ((table[i].nameSector = freeMap->Find()) == -1)
                    return FALSE;
                table[i].short_name = FALSE;
                synchDisk->WriteSector(table[i].nameSector, name);
            }
            table[i].sector = newSector;
            return TRUE;
        }
    return FALSE; // no space.  Fix when we have extensible files.
}

//----------------------------------------------------------------------
// Directory::Remove
// 	Remove a file name from the directory.  Return TRUE if successful;
//	return FALSE if the file isn't in the directory.
//
//	"name" -- the file name to be removed
//----------------------------------------------------------------------

bool Directory::Remove(BitMap *freeMap, char *name)
{
    int i = FindIndex(name);

    if (i == -1)
        return FALSE; // name not in directory
    if (!table[i].short_name)
        freeMap->Clear(table[i].nameSector);
    table[i].inUse = FALSE;
    return TRUE;
}

//----------------------------------------------------------------------
// Directory::GetLongName
// 	Get the long name of current file.
//----------------------------------------------------------------------

void Directory::GetLongName(char *name, int i)
{
    synchDisk->ReadSector(table[i].nameSector, name);
}

//----------------------------------------------------------------------
// Directory::List
// 	List all the file names in the directory.
//----------------------------------------------------------------------

void Directory::List()
{
    char longName[129];
    for (int i = 0; i < tableSize; i++)
        if (table[i].inUse)
        {
            if (table[i].short_name)
                printf("%s\n", table[i].name);
            else
            {
                GetLongName(longName, i);
                printf("%s\n", longName);
            }
        }
}

//----------------------------------------------------------------------
// Directory::Print
// 	List all the file names in the directory, their FileHeader locations,
//	and the contents of each file.  For debugging.
//----------------------------------------------------------------------

void Directory::Print()
{
    FileHeader *hdr = new FileHeader;
    char longName[129];

    printf("\nDirectory contents:\n");
    for (int i = 0; i < tableSize; i++)
        if (table[i].inUse)
        {
            printf("Name: ");
            if (table[i].short_name)
                printf("%s, ", table[i].name);
            else
            {
                GetLongName(longName, i);
                printf("%s, ", longName);
            }
            printf("Sector: %d\n", table[i].sector);
            hdr->FetchFrom(table[i].sector);
            hdr->Print();
        }
    printf("\n");
    delete hdr;
}
