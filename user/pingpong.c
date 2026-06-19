#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int our_pipe[2];
    pipe(our_pipe);

    int f = fork();

    if (f == 0)
    {
        // Child
        char buff;
        read(our_pipe[0], &buff, 1);
        printf("%d: received ping\n", getpid());
        write(our_pipe[1], &buff, 1);
        close(our_pipe[0]);
        close(our_pipe[1]);
    }
    else
    {
        // Parent
        char buff;
        write(our_pipe[1], "A", 1);
        read(our_pipe[0], &buff, 1);
        printf("%d: received pong\n", getpid());
        close(our_pipe[0]);
        close(our_pipe[1]);
    }

    exit(0);
}
