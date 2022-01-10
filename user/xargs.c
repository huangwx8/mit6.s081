#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int read_line(int fd, char* buf)
{
    char* p = buf;
    int n_bytes = 1;
    int length = 0;
    
    for (; ; p++) 
    {
        n_bytes = read(fd, p, sizeof(char));
        if (n_bytes <= 0) 
        {
            exit(1);
        }
        if ((*p) == '\n') 
        {
            break;
        }
        length += n_bytes;
    }
    *p = '\0';
    return length;
}

char** create_argv(int argc, char* argv[], char* extra_args) 
{
    static char* buf[MAXARG];
    int i;
    char* p;

    // copy
    for (i = 0; i < argc; i++) 
    {
        buf[i] = argv[i];
    }

    // split 
    p = extra_args;
    while (1) 
    {
        buf[i++] = p;
        p = strchr(p, ' ');
        if (p) 
        {
            *(p++) = '\0';
        }
        else 
        {
            break;
        }
    }

    if (i >= MAXARG) 
    {
        fprintf(2, "error: too many arguments.\n");
        exit(1);
    }

    buf[i] = 0;

    return buf;
}

int main(int argc, char *argv[])
{
    const char* usage_msg = "Usage: xargs [program] [arguments...]";
    int pid;
    int status;
    int length;
    char buf[512];

    if (argc < 2) 
    {
        fprintf(2, usage_msg);
        exit(0);
    }

    for (; (length = read_line(0, buf)) > 0 ; ) 
    {
        char* prog = argv[1];
        char** prog_argv = create_argv(argc - 1, argv + 1, buf);
        pid = fork();
        if (pid == 0) // child process
        {
            exec(prog, prog_argv);
            exit(0);
        }
        else if (pid > 0) // parent process
        {
            wait(&status);
        }
        else 
        {
            break;
        }
    }

    exit(0);
}
