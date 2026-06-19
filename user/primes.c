#include "kernel/types.h"
#include "user/user.h"
#include <stdbool.h>
#include <stddef.h>

#define N 35

void make_next_prime_pipe(bool is_root, int *pipe_with_left_neighbor)
{

    // Read the first sent number already before the fork (to stop when needed)
    close(pipe_with_left_neighbor[1]);
    int p = 0;
    if (!is_root)
    {
        if (read(pipe_with_left_neighbor[0], &p, 1) <= 0)
        {
            close(pipe_with_left_neighbor[0]);
            close(pipe_with_left_neighbor[1]);
            return;
        }
    }

    int pipe_with_right_neighbor[2];
    pipe(pipe_with_right_neighbor);

    int f = fork();

    if (f == 0)
    {
        // My right neighbor
        close(pipe_with_left_neighbor[0]);
        make_next_prime_pipe(false, pipe_with_right_neighbor);
        return;
    }

    // Me
    close(pipe_with_right_neighbor[0]);

    if (is_root)
    {
        printf("prime 2\n");
        for (int i = 3; i <= N; i += 2)
        {
            write(pipe_with_right_neighbor[1], &i, 1);
        }
    }
    else
    {
        printf("prime %d\n", p);

        int n = 0;
        while (read(pipe_with_left_neighbor[0], &n, 1) > 0)
        {

            if (n % p != 0)
            {
                write(pipe_with_right_neighbor[1], &n, 1);
            }
        }

        close(pipe_with_left_neighbor[0]);
    }
    close(pipe_with_right_neighbor[1]);
}

int main(int argc, char *argv[])
{
    make_next_prime_pipe(true, NULL);
    wait(NULL);
    exit(0);
}
