#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include "../../kernel/logger.hpp"

// auto &printk = *reinterpret_cast<int (*)(const char *, ...)>(0x000000000010b6f0);
// auto &fill_rect = *reinterpret_cast<decltype(FillRectangle) *>(0x000000000010fd90);
// auto &scrn_writer = *reinterpret_cast<decltype(screen_writer) *>(0x0000000000271058);

int stack_ptr;
long stack[100];

long Pop()
{
    long value = stack[stack_ptr];
    stack_ptr--;
    return value;
}

void Push(long value)
{
    stack_ptr++;
    stack[stack_ptr] = value;
}

extern "C" int64_t SyscallLogString(LogLevel, const char *);


extern "C" int main(int argc, char **argv)
{
    stack_ptr = -1;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "+") == 0)
        {
            long b = Pop();
            long a = Pop();
            Push(a + b);
            // printk("[%d] <- %ld\n", stack_ptr, a + b);
            SyscallLogString(kWarn, "+");
        }
        else if (strcmp(argv[i], "-") == 0)
        {
            long b = Pop();
            long a = Pop();
            Push(a - b);
            // printk("[%d] <- %ld\n", stack_ptr, a - b);
            SyscallLogString(kWarn, "-");
        }
        else
        {
            long a = atol(argv[i]);
            Push(a);
            SyscallLogString(kWarn, "#");
        }
    }
    // fill_rect(*scrn_writer, Vector2D<int>{100, 10}, Vector2D<int>{200, 200}, ToColor(0x00ff00));

    if (stack_ptr < 0)
    {
        return 0;
    }
    long result = 0;
    if(stack_ptr>=0){
        result = Pop();
    }
    printf("%ld\n", result);
    while (1)
        ;
    // return static_cast<int>(Pop());
}