#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  int p[2];
  pipe(p);
  printf("p0 has now result: %d \n", p[0]);
  printf("p1 has now result: %d \n", p[1]);

  if (fork() == 0)
  {
    char buf[10];
    read(p[0], buf, sizeof buf);
    int id = getpid();
    printf("%d : child received %s \n", id, buf);
    write(p[1], "pong", 4);
    close(p[0]);
    close(p[1]);
  }
  else
  {
    char buf[10];
    int id = getpid();
    write(p[1], "ping", 4);
    close(p[1]);
    wait();
    read(p[0], buf, sizeof buf);
    printf("%d : parent received %s \n", id, buf);
    close(p[0]);
  }

  exit();
}