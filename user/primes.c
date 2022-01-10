#include "kernel/types.h"
#include "user/user.h"

// error report in case of fork failure
const char* fork_failure = "fork failed, no free process\n";

// safe fork
int fork_s() 
{
    int pid;
    pid = fork();
    if (pid < 0) // fork failed
    {
        printf(fork_failure);
        exit(1);
    }
    return pid;
}

/** 
 * Doug McIlroy's loop
 * 
 * p = get a number from left neighbor
 * print p
 * loop:
 *    n = get a number from left neighbor
 *    if (p does not divide n)
 *        send n to right neighbor
 */
void worker(int source_fd)
{
    int n_bytes;
    int prime;
    int num;
    int pid = 0;
    int fds[2];
    int status;
    
    n_bytes = read(source_fd, &prime, sizeof(int));
    printf("prime %d\n", prime);

    while (1) 
    {
        // try read
        n_bytes = read(source_fd, &num, sizeof(int));
        // meet an eof, stop worker
        if (n_bytes <= 0)
        {
            break;
        }
        // potential prime
        if (num % prime != 0) 
        {
            // create a child process(and a pipe) at the first time
            if (pid == 0)
            {
                pipe(fds);
                pid = fork_s();
                if (pid == 0) // child process
                {
                    close(fds[1]);
                    worker(fds[0]);
                }
                else // parent process
                {
                    close(fds[0]);
                }
            }
            write(fds[1], &num, sizeof(int));
        }
    }
    // release resource
    close(source_fd);
    if (pid > 0)
    {
        close(fds[1]);
        wait(&status);
    }
    exit(0);
}

int main(int argc, char *argv[])
{
    int num;
    int pid;
    int status;
    int fds[2];

    // create pipe
    pipe(fds);

    // create child process
    pid = fork_s();

    if (pid == 0) // child process
    {
        // read from pipe only
        close(fds[1]);
        // do Doug McIlroy's loop
        worker(fds[0]);
    }
    else // parent process
    {
        // write to pipe only
        close(fds[0]);
        // write integer 2~35 in turn
        for (num = 2; num <= 35; num++) 
        {
            write(fds[1], &num, sizeof(int));
        }
        // do not need to write anymore
        close(fds[1]);
        // terminate child process
        wait(&status);
    }
    
    exit(0);
}
