#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  int parent_fd[2];
  int child_fd[2];
  pipe(parent_fd);
  pipe(child_fd);

  if (fork() == 0)
  {
    char buf[10];
    read(parent_fd[0], buf, sizeof buf);
    int id = getpid();
    printf("%d: received %s\n", id, buf);
    write(child_fd[1], "pong", 4);
    close(child_fd[0]);
    close(child_fd[1]);
  }
  else
  {
    char buf[10];
    int id = getpid();
    write(parent_fd[1], "ping", 4);
    close(parent_fd[1]);
    wait();
    read(child_fd[0], buf, sizeof buf);
    printf("%d: received %s\n", id, buf);
    close(child_fd[0]);
  }

  exit();
}