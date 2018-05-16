#include "syscall.h"

int id;

void func()
{
	Yield();
	Exit(1);
}

int main()
{
	Fork(func);
	Yield();
	id = Exec("../test/sort");
	Yield();
	Join(id);	
	Exit(0);
}