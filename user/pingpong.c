#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    // process id
    int pid;
    // process return status
    int status;
    // ping pong table
    int parent_pipe[2];
    int child_pipe[2];
    // ping pong table
    char ball = 'o';
    // error report in case of fork failure
    const char* fork_failure = "fork failed, no free process\n";

    // create pipes
    pipe(parent_pipe);
    pipe(child_pipe);

    // create a child process
    pid = fork();

    if (pid == 0) // child process
    {
        write(parent_pipe[1], &ball, 1);
        read(child_pipe[0], &ball, 1);
        printf("%d: received pong\n", getpid());
    }
    else if (pid > 0) // parent process
    {
        read(parent_pipe[0], &ball, 1);
        printf("%d: received ping\n", getpid());
        write(child_pipe[1], &ball, 1);
        wait(&status);
    }
    else // fork failed
    {
        fprintf(2, fork_failure);
        exit(1);
    }

    exit(0);
}
