/* library source for new system calls */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <sys/types.h>

extern int errno;

#define __NR_dub		 41

int dub(int fd) {
  return syscall(__NR_dub, fd);
}

/* The following would appear in the caller's program...

main()
{
  dub(1);
}


*/
