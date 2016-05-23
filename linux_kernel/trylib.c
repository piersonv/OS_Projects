/* test program for lib.c */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "lib.h"

int main()
{
  int rv;
  char *msg = "Hello, world!\n";
  printf("dub(1) returns %d\n", rv = dub(1));

  printf("Test of write on fd %d:\n", rv);
  write(rv, msg, strlen(msg));

  return 0;
}
