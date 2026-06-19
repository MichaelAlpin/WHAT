#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"
#include "kernel/fs.h"

#include <stddef.h>

#define MAX_LINE_LEN 128

void xargs(int argc, char *argv[])
{
    char line[MAX_LINE_LEN];
    char *new_argv[MAXARG];

    // Copy the arguments
    int i = 0;
    for (; argv[i + 1] != NULL; i++)
    {
        new_argv[i] = argv[i + 1];
    }
    new_argv[i] = NULL;
    new_argv[i + 1] = NULL;

    while (strlen(gets(line, MAX_LINE_LEN)) > 0)
    {
        // Set the new argument to be the read line
        line[strlen(line) - 1] = '\0';
        new_argv[i] = line;

        // Create the child process running the command
        int f = fork();

        if (f == 0)
        {
            // Child
            exec(new_argv[0], new_argv);
        }
        else
        {
            // Parent
            wait(NULL);
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(2, "Usage: xargs command\n");
        exit(1);
    }

    xargs(argc, argv);

    exit(0);
}
