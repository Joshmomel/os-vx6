#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  int argv_len = 0;
  char buf[128] = {'\0'};
  char *new_argv[MAXARG];

  // 把argv的内容拷贝到new_argv
  for (int i = 1; i < argc; i++)
  {
    new_argv[i - 1] = argv[i];
  }

  // 读取user输入
  while (gets(buf, sizeof(buf)))
  {
    int buf_len = strlen(buf);
    printf("buf length is %d\n", buf_len);
    if (buf_len < 1)
      break;
    argv_len = argc - 1;
    buf[buf_len - 1] = '\0';

    // 把buf中读取到用户输入的内容按照word拆分到每个new_argv中
    for (char *p = buf; *p; p++)
    {
      printf("in the loop, p is %s\n", p);
      while (*p && (*p == ' '))
      {
        *p++ = '\0';
      }
      if (*p)
      {
        new_argv[argv_len++] = p;
      }
      while (*p && (*p != ' '))
      {
        p++;
      }
    }

    // 终止string
    new_argv[argv_len] = "\0";

    // 父进程
    if (fork() > 0)
    {
      wait();
    }
    else
    {
      printf("exec start: \n");
      exec(new_argv[0], new_argv);
    }
  }

  exit();
}