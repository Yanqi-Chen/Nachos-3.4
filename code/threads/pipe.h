#include "synch.h"

#define BUFSIZE 256

class Condition;
class Lock;

class Pipe
{
  public:
	Pipe();
	~Pipe();

	void PutChar(char ch);
	char GetChar();

	int pos;  

  private:
	char buffer[BUFSIZE];
	Condition *cond; 
	Lock *full;
	Lock *empty;
};


