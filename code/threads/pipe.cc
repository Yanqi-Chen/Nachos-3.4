#include "system.h"

Pipe::Pipe()
{
    pos = 0;
    cond = new Condition("pipe cond");
    full = new Lock("full lock");
    empty = new Lock("empty lock");
}

Pipe::~Pipe()
{
    delete cond;
    delete full;
    delete empty;
}

char Pipe::GetChar()
{
    while (pos == 0)
        cond->Wait(empty);

    char c = buffer[--pos];
    cond->Signal(full);
    return c;
}

void Pipe::PutChar(char ch)
{
    while (pos == BUFSIZE)
        cond->Wait(full);

    buffer[pos++] = ch;
    cond->Signal(empty);
}
