#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "user/user.h"

int main(void)
{
    printf("x=%d y=%d", 3);

    return 0;
}