#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstring>
#include <cstdlib>

using namespace std;

const int MAX_ARGS = 100;

enum Direction {PIPE, PIPE2, REDIRECT, BREAK, NOTHING}; //list of options user takes

//function declarations
int read(char**);
Direction parse(int, char**, char**, char**, char**);
void pipe1(char**, char**);
void pipe2(char**, char**, char**);
void redirect(char**, char**);
void execute(int, char**);

int main() {
	char *argv[MAX_ARGS], *cmd1[MAX_ARGS], *cmd2[MAX_ARGS], *cmd3[MAX_ARGS];
	Direction cmd_type;
	int count;

	while (true) //loop until user quits
	{
		cout << "MyShell> ";

		count = read(argv);

		cmd_type = parse(count, argv, cmd1, cmd2, cmd3);

		//goes to different functions based on what's in the command
		if (cmd_type == PIPE)
			pipe1(cmd1, cmd2);
		else if (cmd_type == REDIRECT)
			redirect(cmd1, cmd2);
		else if (cmd_type == PIPE2)
			pipe2(cmd1, cmd2, cmd3);
		else if (cmd_type == BREAK)
			continue;
		else
			execute(count, argv);

		for (int i = 0; i < count; i++)
			argv[i] = NULL;
	}

	return 0;
}

//executes a single command that doesn't have anything special added to it
void execute(int count, char** argv)
{
	pid_t pid = fork();
	const char *amp = "&";
	bool amp_found = false;
	
	//if there is an ampersand as last argument
	if (strcmp(argv[count-1], amp) == 0)
		amp_found = true;

	if (pid < 0)
		perror("Error: (pid < 0)");

	else if (pid == 0) 
	{
		//remove ampersand
		if (amp_found) {
			argv[count-1] = NULL;
			count--;
		}

		execvp(argv[0], argv);
		perror("Error: (execvp)");
	}
	
	else if (!amp_found)
		waitpid(pid, NULL, 0);
}

//redirection function
void redirect(char** cmd, char** file)
{
	int fds[2], cnt, fd;
	char character;
	pid_t pid;

	pipe(fds); //create pipe with file descripters
	
	//first child
	if (fork() == 0) 
	{
		fd = open(file[0], O_RDWR | O_CREAT, 0666); //headers
		
		if (fd < 0) {
		printf("Error: (fd < 0)");
		return;
		}

		dup2(fds[0], 0);
		
		close(fds[1]);
		
		//reading in from file
		while ((cnt = read(0, &character, 1)) > 0)
			write(fd, &character, 1);

		execlp("echo", "echo", NULL);
	}
	//second child process
	else if ((pid = fork()) == 0)
	{
		dup2(fds[1], 1);
		close(fds[0]);

		//output contents
		execvp(cmd[0], cmd);
		perror("Error: (execvp)");
	}
	//parent process
	else
	{
		waitpid(pid, NULL, 0);
		close(fds[0]);
		close(fds[1]);
	}
}

//pipe process if only one pipe
void pipe1(char** cmd1, char** cmd2)
{
	int fds[2];
	pipe(fds);
	pid_t pid1, pid2;
	
	//first child
	if (pid1 = fork() == 0)
	{
		dup2(fds[1], 1);
		close(fds[0]);
		close(fds[1]);
		execvp(cmd1[0], cmd1);
		perror("Error: (execvp)");
	}
	//second child
	else if((pid2 = fork()) == 0)
	{
		dup2(fds[0], 0);

		close(fds[1]);
		execvp(cmd2[0], cmd2);
		perror("Error: (execvp)");
	}
	//parent process
	else
	{
		close(fds[1]);
		waitpid(pid1, NULL, 0);
		waitpid(pid2, NULL, 0);
	}
}

//pipe process if two pipes
void pipe2(char** cmd1, char** cmd2, char** cmd3)
{
	int fds[2];
	pipe(fds);
	pid_t pid1, pid2, pid3;
	
	//first child
	if ((pid1 = fork()) == 0)
	{
		dup2(fds[1], 1);
		close(fds[0]);
		close(fds[1]);
		execvp(cmd1[0], cmd1);
		perror("Error: (execvp)");
	}
	//second child
	else if ((pid2 = fork()) == 0)
	{
		dup2(fds[0], 0);
		
		execvp(cmd2[0], cmd2);
		perror("Error: (execvp)");
	}
	//third child
	else if ((pid3 = fork()) == 0)
	{
		dup2(fds[0], 0);
		close(fds[0]);

		execvp(cmd3[0], cmd3);
		perror("Error: (execvp)");
	}
	//parent process
	else
	{
		//close(fds[0]);
		close(fds[1]);
		waitpid(pid1, NULL, 0);
		waitpid(pid2, NULL, 0);
		waitpid(pid3, NULL, 0);
	}
}

//longest function that parses through arguments
Direction parse(int count, char** argv, char** cmd1, char** cmd2, char** cmd3)
{
	Direction result = NOTHING;

	int split1 = -1, split2 = -1;

	for (int i = 0; i < count; i++)
	{
		if(split1 == -1) //if there has been no split yet
		{
			if (strcmp(argv[i], "|") == 0) {
				result = PIPE;
				split1 = i;
			}
			else if (strcmp(argv[i], ">>") == 0) {
				result = REDIRECT;
				split1 = i;
			}
			else if (strcmp(argv[i], ";") == 0) {
				result = BREAK;
				split1 = i;
			}
		}
		else //if there has already been a split
		{
			if (strcmp(argv[i], "|") == 0) {
				result = PIPE2;
				split2 = i;
			}
		}
	}

	if (result != NOTHING) //if there was a pipe or redirection
	{
		if (result == PIPE2) { //if there were two pipes
			//filter for three commands
			for (int i = 0; i < split1; i++)
				cmd1[i] = argv[i];

			int count1 = 0;
			for (int i = split1 + 1; i < split2; i++) {
				cmd2[count1] = argv[i];
				count1++;
			}

			int count2 = 0;
			for (int i = split2 + 1; i < count; i++) {
				cmd3[count2] = argv[i];
				count2++;
			}

			cmd1[split1] = NULL;
			cmd2[count1] = NULL;
			cmd3[count2] = NULL;
		}
		else {
			//only filter for two commands
			for (int i = 0; i < split1; i++)
				cmd1[i] = argv[i];

			int cnt = 0;
			for (int i = split1+1; i < count; i++) {
				cmd2[cnt] = argv[i];
				cnt++;
			}
			
			//place NULL at the end so it can work with execvp
			cmd1[split1] = NULL;
			cmd2[cnt] = NULL;

			if (result == BREAK) { //if there was a break, run each command seperately
				execute(split1, cmd1);
				execute(cnt, cmd2);
			}
		}
	}

	return result;
}

// reads arguments, counts them, and checks if user wants to quit
int read(char** argv) 
{
	char *cstr;
	string arg;
	int count = 0;

	while (cin >> arg)
	{
		for (int i = 0; i < arg.length(); i++)
			arg[i] = tolower(arg[i]);
		
		if (arg == "quit") //if user types in quit, exit shell
			exit(0);
		
		//converts string into C string format
		cstr = new char[arg.size()+1];
		strcpy(cstr, arg.c_str());
		argv[count] = cstr;

		count++;
		
		if (cin.get() == '\n') //if user presses enter, stop
			break;
	}

	argv[count] = NULL;

	return count;
}
