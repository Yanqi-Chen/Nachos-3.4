
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.


#include "copyright.h"
#include "utility.h"
#include "console.h"
#include "synch.h"

// The following class defines a hardware console device.
// Input and output to the device is simulated by reading 
// and writing to UNIX files ("readFile" and "writeFile").
//
// Since the device is asynchronous, the interrupt handler "readAvail" 
// is called when a character has arrived, ready to be read in.
// The interrupt handler "writeDone" is called when an output character 
// has been "put", so that the next character can be written.

class SynchConsole {
  public:
    SynchConsole(char *readFile, char *writeFile);
				// initialize the hardware console device
    ~SynchConsole();			// clean up console emulation

// external interface -- Nachos kernel code can call these
    void PutChar(char ch);	// Write "ch" to the console display, 
				// and return immediately.  "writeHandler" 
				// is called when the I/O completes. 

    char GetChar();	   	// Poll the console input.  If a char is 
				// available, return it.  Otherwise, return EOF.
    				// "readHandler" is called whenever there is 
				// a char to be gotten

  private:
    Console *console;    	
	Lock *lock;	
};

