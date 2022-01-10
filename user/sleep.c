#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    const char* usage_msg = "Usage: sleep n_ticks\n";
    int n_ticks;
    /* Except exactly one argument */
    if (argc != 2)
    {
        fprintf(2, usage_msg);
        exit(1);
    }
    n_ticks = atoi(argv[1]);
    sleep(n_ticks);
    exit(0);
}
