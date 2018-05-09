// filesys.cc
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   there is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "disk.h"
#include "bitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"
#include "system.h"
#include <string.h>

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known
// sectors, so that they can be located on boot-up.
#define FreeMapSector 0
#define DirectorySector 1

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number
// of files that can be loaded onto the disk.
#define FreeMapFileSize (NumSectors / BitsInByte)
#define NumDirEntries 10
#define DirectoryFileSize (sizeof(DirectoryEntry) * NumDirEntries)

//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------

FileSystem::FileSystem(bool format)
{
    printf("Using real nachos file system\n");
    DEBUG('f', "Initializing the file system.\n");
    if (format)
    {
        BitMap *freeMap = new BitMap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
        FileHeader *mapHdr = new FileHeader;
        FileHeader *dirHdr = new FileHeader;

        DEBUG('f', "Formatting the file system.\n");

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
        freeMap->Mark(FreeMapSector);
        freeMap->Mark(DirectorySector);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize));
        ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize));

        // Flush the bitmap and directory FileHeaders back to disk
        // We need to do this before we can "Open" the file, since open
        // reads the file header off of disk (and currently the disk has garbage
        // on it!).

        DEBUG('f', "Writing headers back to disk.\n");
        mapHdr->WriteBack(FreeMapSector);
        dirHdr->WriteBack(DirectorySector);

        // OK to open the bitmap and directory files now
        // The file system operations assume these two files are left open
        // while Nachos is running.

        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);

        // Once we have the files "open", we can write the initial version
        // of each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
        freeMap->WriteBack(freeMapFile); // flush changes to disk
        directory->WriteBack(directoryFile);
        if (DebugIsEnabled('f'))
        {
            freeMap->Print();
            directory->Print();

            delete freeMap;
            delete directory;
            delete mapHdr;
            delete dirHdr;
        }
    }
    else
    {
        // if we are not formatting the disk, just open the files representing
        // the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
    }
}

//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//----------------------------------------------------------------------

bool FileSystem::Create(char *name, int initialSize, OpenFile *dFile = NULL, BitMap *freeMap = NULL, int depth = 0)
{
    // Father directory
    Directory *directory;
    FileHeader *hdr;
    int sector;
    bool success = TRUE;
    bool direc_exist = FALSE;
    //
    bool is_directory = FALSE;
    int nameLen = strlen(name);
    char *filename = new char[nameLen];
    char *pathname = new char[nameLen];
    int tableID;
    char *filenameBegin = name, *filenameEnd = NULL;

    if (freeMap == NULL)
    {
        freeMap = new BitMap(NumSectors);
        freeMap->FetchFrom(freeMapFile);
    }
    if (dFile == NULL)
    {
        dFile = directoryFile;
    }
    for (int i = 0; i < depth; i++)
    {
        filenameBegin = strchr(filenameBegin, '/');
        filenameBegin++;
    }
    filenameEnd = strchr(filenameBegin, '/');
    if (filenameEnd == NULL)
    {
        is_directory = FALSE;
        strcpy(filename, filenameBegin);
        strncpy(pathname, name, strlen(name));
        pathname[strlen(name)] = '\0';
    }
    else
    {
        is_directory = TRUE;
        strncpy(filename, filenameBegin, filenameEnd - filenameBegin);
        filename[filenameEnd - filenameBegin] = '\0';
        strncpy(pathname, name, filenameEnd - name);
        pathname[filenameEnd - name] = '\0';
    }
    // printf("%s\n", name);
    // printf("%s\n", filename);
    // printf("%s\n\n", pathname);
    //
    if (is_directory)
    {
        int childDirectorySector = -1;
        directory = new Directory(NumDirEntries);
        directory->FetchFrom(dFile);
        if (directory->Find(filename) == -1)
            success = FALSE;
        // Find header sector of child directory if existed
        childDirectorySector = directory->Find(filename);
        if (childDirectorySector == -1)
        {
            childDirectorySector = freeMap->Find();
            if (childDirectorySector == -1)
                success = FALSE;
            else if (!directory->Add(freeMap, filename, pathname, childDirectorySector))
                success = FALSE;
            else
            {
                success = TRUE;
                hdr = new FileHeader;
            }
        }
        // There exist a file or a directory with the same name
        else
        {
            hdr->FetchFrom(childDirectorySector);
            // If a file, failed
            if (!hdr->GetIfDir())
            {
                success = FALSE;
                printf("Failed! There has existed a file named \"%s\"", filename);
            }
            direc_exist = TRUE;
        }
        if (success)
        {
            // directory exists
            tableID = directory->FindIndex(filename);
            Directory *dir = new Directory(NumDirEntries);
            printf("%d\n",direc_exist);
            if (!direc_exist)
            {
                if (!hdr->Allocate(freeMap, DirectoryFileSize))
                    success = FALSE;
            }   
            if (success)
            {
                hdr->SetIfDir(true);
                hdr->SetCreateTime();
                hdr->SetModTime();
                hdr->SetUsedTime();
                hdr->SetType("");
                hdr->WriteBack(childDirectorySector);
                directory->WriteBack(dFile);
                OpenFile *childDirectoryFile = new OpenFile(childDirectorySector);
                Create(name, initialSize, childDirectoryFile, freeMap, depth + 1);
                delete hdr;
                delete childDirectoryFile;
                // ReachrRoot directory, create complete
                if (dFile == directoryFile)
                {
                    freeMap->WriteBack(freeMapFile);
                    delete freeMap;
                }
            }
        }
    }
    else
    {
        DEBUG('f', "Creating file %s, size %d\n", filename, initialSize);

        directory = new Directory(NumDirEntries);
        directory->FetchFrom(dFile);

        if (directory->Find(filename) != -1)
        {
            success = FALSE; // file is already in directory
        }
        else
        {
            sector = freeMap->Find(); // find a sector to hold the file header
            if (sector == -1)
            {
                success = FALSE; // no free block for file header
            }
            else if (!directory->Add(freeMap, filename, pathname, sector))
            {
                success = FALSE; // no space in directory
            }
            else
            {
                printf("%s\n", filename);
                hdr = new FileHeader;
                if (!hdr->Allocate(freeMap, initialSize))
                    success = FALSE; // no space on disk for data
                else
                {
                    success = TRUE;
                    /* lab5 begin */
                    hdr->SetIfDir(false);
                    hdr->SetCreateTime();
                    hdr->SetModTime();
                    hdr->SetUsedTime();
                    hdr->SetType(filename);
                    /* lab5 end */
                    // everthing worked, flush all changes back to disk
                    hdr->WriteBack(sector);
                    directory->WriteBack(dFile);
                    freeMap->WriteBack(freeMapFile);
                }
                delete hdr;
            }
        }
    }
    delete directory;
    return success;
}

//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.
//	To open a file:
//	  Find the location of the file's header, using the directory
//	  Bring the header into memory
//
//	"name" -- the text name of the file to be opened
//----------------------------------------------------------------------

OpenFile *
FileSystem::Open(char *name)
{
    Directory *directory = new Directory(NumDirEntries);
    OpenFile *openFile = NULL;
    int sector;

    DEBUG('f', "Opening file %s\n", name);
    directory->FetchFrom(directoryFile);
    sector = directory->Find(name);
    if (sector >= 0)
        openFile = new OpenFile(sector); // name was found in directory
    delete directory;
    return openFile; // return NULL if not found
}

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------

bool FileSystem::Remove(char *name, OpenFile *dFile = NULL)
{
    printf("%s\n", name);
    Directory *directory;
    BitMap *freeMap;
    FileHeader *fileHdr;
    int sector;
    if (dFile == NULL)
        dFile = directoryFile;

    directory = new Directory(NumDirEntries);
    directory->FetchFrom(dFile);
    sector = directory->Find(name);

    int depth = 0;
    int nameLen = strlen(name);

    char *filename = new char[nameLen];
    char *dirname = new char[nameLen];

    char *p = strrchr(name, '/');
    if (p)
    {
        strncpy(dirname, name, p - name);
        dirname[p - name]= '\0';
        strcpy(filename, p + 1);
        int dsector = directory->Find(dirname);
        dFile = new OpenFile(dsector);
        printf("%d\n", dsector);
        directory->FetchFrom(dFile);
    }
    else
        strcpy(filename, name);
   
    if (sector == -1)
    {
        printf("File \"%s\" not found\n", name);
        delete directory;
        return false;
    }
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);
    if (fileHdr->GetIfDir())
    {
        OpenFile *tempDir = new OpenFile(sector);
        Directory *dir = new Directory(NumDirEntries);
        dir->FetchFrom(tempDir);
        for (int i = 0; i < NumDirEntries; ++i)
        {
            if (dir->GetEntry(i).inUse)
            {
                char *tempName = new char[nameLen];
                dir->GetLongName(tempName, i);
                Remove(tempName, tempDir);
                delete[] tempName;
                dir->GetEntry(i).inUse = FALSE;
            }
        }
        delete dir;
        delete tempDir;
    }

    Directory *ddirectory = new Directory(NumDirEntries);
    ddirectory->FetchFrom(directoryFile);

    freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);
    fileHdr->Deallocate(freeMap);
    freeMap->Clear(sector);

    directory->Remove(freeMap, filename);

    freeMap->WriteBack(freeMapFile);
    directory->WriteBack(dFile);
    delete fileHdr;
    delete directory;
    delete[] filename;
    delete dFile;
    delete freeMap;
    return true;
}

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------

void FileSystem::List()
{
    Directory *directory = new Directory(NumDirEntries);

    directory->FetchFrom(directoryFile);
    directory->List();
    delete directory;
}

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------

void FileSystem::Print()
{
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    BitMap *freeMap = new BitMap(NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print();

    printf("Directory file header:\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print();

    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();

    directory->FetchFrom(directoryFile);
    directory->Print();

    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
}
