#include "syscall.h"

int main()
{
	SpaceId newProc;
	OpenFileId input = ConsoleInput;
	OpenFileId output = ConsoleOutput;
	char prompt[2], ch, buffer[60];
	char endl[1];
	int len;

	endl[0] = '\n';
	prompt[0] = '$';
	prompt[1] = ' ';
	

	while (1)
	{
		Write(prompt, 3, output);

		len = 0;

		do
		{
			Read(&buffer[len], 1, input);
		} while (buffer[len++] != '\n');

		buffer[--len] = '\0';

		if (buffer[0] == 'e' && buffer[1] == 'c')
		{
			Write(buffer + 5, len - 5, output);
			Write(endl, 1, output);
		}
		else if (buffer[0] == 'e' && buffer[1] == 'x')
			Exit(0);
		else if (buffer[0] == 'l')
			Ls();
		else if (buffer[0] == 'p' && buffer[1] == 'w')
			Pwd();
		else if (buffer[0] == 'p' && buffer[1] == 's')
			Ps();
		else if (buffer[0] == 'c')
		{
			int i;
			for (i = 0; buffer[i] != ' '; ++i) {};
			Chdir(buffer + i + 1);
		}
		else if (len > 0)
		{
			newProc = Exec(buffer);
			Yield();
			Join(newProc);
		}
	}
}
