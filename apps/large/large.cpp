#include <cstdlib>
#include "../syscall.h"

char table[3 * 1024 * 1024];

extern "C" void main(int argc, char **argv)
{
    int exit_code = -1;
    if (argc > 1)
    {
        exit_code = atoi(argv[1]);
    }
    SyscallExit(exit_code);
}