#include "syscall.h"

char buf[15];
char fname[5] = "test";
char str[15] = "Hello world!";

int main()
{
	int fd1, fd2, numRead;

	Create(fname);

	fd1 = Open(fname);
	Write(str, 13, fd1);
	Close(fd1);

	fd2 = Open(fname);
	numRead = Read(buf, 13, fd2);
	Close(fd2);

	Exit(numRead);
}