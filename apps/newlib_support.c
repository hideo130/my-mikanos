#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "syscall.h"

int close(int fd)
{
  errno = EBADF;
  return -1;
}

int fstat(int fd, struct stat *buf)
{
  errno = EBADF;
  return -1;
}

int isatty(int fd)
{
  errno = EBADF;
  return -1;
}

off_t lseek(int fd, off_t offset, int whence)
{
  errno = EBADF;
  return -1;
}

ssize_t read(int fd, void *buf, size_t count)
{
  errno = EBADF;
  return -1;
}

caddr_t sbrk(int incr)
{
  errno = ENOMEM;
  return (caddr_t)-1;
}

ssize_t write(int fd, const void *buf, size_t count)
{
  struct SyscallResult res = SyscallPutString(fd, buf, count);
  if (res.error == 0)
  {
    return res.value;
  }
  errno = res.error;
  return -1;
}

void _exit(int status)
{
  SyscallExit(status);
}

int getpid(void)
{
  return 1;
}

int kill(int pid, int sig)
{
  errno = EINVAL;
  return -1;
}

int posix_memalign(void **memptr, size_t alignment, size_t size)
{
  errno = EINVAL;
  return -1;
}

void __gxx_personality_v0()
{
  errno = EPERM;
}

void __cxa_begin_catch()
{
  errno = EPERM;
}