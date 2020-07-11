#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  printf("prime function is called \n");

  int first, v;

  int *fd1;
  int *fd2;

  int first_fd[2];
  int second_fd[2];

  pipe(first_fd);

  // 在父进程中
  if (fork() > 0)
  {
    for (int i = 2; i <= 35; i++)
    {
      write(first_fd[1], &i, sizeof(i));
    }
    // 关闭standard output: write
    close(first_fd[1]);
    wait();
  }
  // 在子进程中 需要循环生成子子进程直到最终的进程没有没有可读的数字
  else
  {
    // 子进程的读来自于上一个进程的fd
    fd1 = first_fd;
    // 子进程的写是一个新的fd
    fd2 = second_fd;

    while (1)
    {
      // 创建pipe为下个读取做准备
      pipe(fd2);
      // 关闭上一个fd的写
      close(fd1[1]);
      // 这时候如果有值，就printf第一个值
      if (read(fd1[0], &first, sizeof(first)))
      {
        printf("prime %d\n", first);
      }
      else
      {
        break;
      }
      // 创建一个进程，如果当前的父进程
      if (fork() > 0)
      {
        // 关闭前一个进程fd的stdin
        // close(fd2[0]);
        // 读取第一个数字, 存到v中
        int i = 0;
        while (read(fd1[0], &v, sizeof(v)))
        {
          // 如果能被整除就读取下一个
          if (v % first == 0)
          {
            continue;
          }
          i++;
          // 如果不能整除，就通过pipe写到下一个进程中作为读取的值
          write(fd2[1], &v, sizeof(v));
        }
        close(fd1[0]);
        close(fd2[1]);
        wait();
        break;
      }
      // 如果在子进程中, 创建新的write，原来的write变成read
      else
      {
        close(fd1[0]);
        int *tmp = fd1;
        fd1 = fd2;
        fd2 = tmp;
      }
    }
  }

  exit();
}