/*
  Lab参考了 
  user/sh.c
  https://github.com/ChyuWei/xv6-riscv-fall19/blob/sh/user/nsh.c#L81
  https://www.cnblogs.com/nlp-in-shell/p/12024806.html
*/

#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define MAX_CMD 8
#define MAXARGS 10
#define BUF_SIZE 1024

struct cmd
{
  char *args[MAXARGS];
  char argc;
  char *input;
  char *output;
};

char whitespace_and_symbols[] = " \t\r\n\v<|>&;()";

void init_cmd(struct cmd *cmd, int index);
int skip(char *token, const char *given_string);
char *trim_space(char *str);

/*
  把buf的char读取出来 存到cmd的struct上
*/
int parse_cmd(struct cmd *cmd, char *buf)
{
  /*
    这是返回值 返回一共有多上个command
    一个pipe等于两个command
  */
  int read_cmd_size = 1;

  char *p = buf;
  while (*p)
  {

    int cmd_index = read_cmd_size - 1;

    p = trim_space(p);

    init_cmd(cmd, cmd_index);

    /*
      处理args
    */
    while (*p && skip(whitespace_and_symbols, p))
    {
      cmd[cmd_index].argc++;
      cmd[cmd_index].args[cmd[cmd_index].argc - 1] = p;
      while (*p && skip(whitespace_and_symbols, p))
        p++;
      while (*p && *p == ' ')
        *p++ = 0;
    }

    /*
      处理重定向
    */
    while (*p == '<' || *p == '>')
    {
      char redirect = *p++;
      while (*p && *p == ' ')
        p++;
      if (redirect == '<')
        cmd[cmd_index].input = p;
      else
        cmd[cmd_index].output = p;
      while (*p && skip(whitespace_and_symbols, p))
        p++;
      while (*p && *p == ' ')
        *p++ = 0;
    }

    /*
      处理pipe
    */
    if (*p == '|')
    {
      p++;
      if (cmd[cmd_index].argc > 0)
        read_cmd_size++;
      else
      {
        fprintf(2, "bad pipe\n");
        return 0;
      }
    }
  }
  return read_cmd_size;
}

void ecec_cmd(struct cmd *cmd)
{
  if (cmd[0].input)
  {
    close(0);
    open(cmd[0].input, O_RDONLY);
  }
  if (cmd[0].output)
  {
    close(1);
    open(cmd[0].output, O_WRONLY | O_CREATE);
  }
  exec(cmd[0].args[0], cmd[0].args);
  exit(0);
}

void run_cmd(struct cmd *cmd, int cmd_size)
{
  void redirect(int k, int pd[]);
  if (cmd_size)
  {
    int pd[2];
    pipe(pd);

    if (fork() > 0)
    {
      /*
        如果有pipe那么做重定向，把当前I/O定向到1
      */
      if (cmd_size > 1)
        redirect(1, pd);
      /*
        执行cmd[0]
      */
      ecec_cmd(cmd);
    }
    else if (fork() > 0)
    {
      /*
        如果有pipe那么做重定向 子进程需要把当前I/O定向到0 并把cmd往前进一个运行
      */
      if (cmd_size > 1)
      {
        redirect(0, pd);
        cmd_size = cmd_size - 1;
        cmd = cmd + 1;
        run_cmd(cmd, cmd_size);
      }
    }
    close(pd[0]);
    close(pd[1]);
    wait(0);
    wait(0);
  }
  exit(0);
}

int getcmd(char *buf, int nbuf)
{
  printf("@ ");
  gets(buf, BUF_SIZE);
  int read_buf_sz = strlen(buf);
  if (read_buf_sz < 1)
    return -1;
  *strchr(buf, '\n') = '\0';
  return 0;
}

/*
  处理space
*/
char *trim_space(char *str)
{
  while (*str && *str == ' ')
  {
    *str++ = 0;
  }
  return str;
}

/*
  给cmd[MAXARGS]对应的内存空间并初始为结束符
*/
void init_cmd(struct cmd *cmd, int index)
{
  cmd[index].argc = 0;
  for (int i = 0; i < MAXARGS; i++)
    cmd[index].args[i] = 0;
}

int skip(char *token, const char *given_string)
{
  return !strchr(whitespace_and_symbols, *given_string);
}

/*
  把pipe的file discriptor 重定向到k端口
*/
void redirect(int k, int pd[])
{
  close(k);
  dup(pd[k]);
  close(pd[0]);
  close(pd[1]);
}

int main(int argc, char *argv[])
{
  static char buf[BUF_SIZE];
  static struct cmd cmd[MAX_CMD];

  while (getcmd(buf, sizeof(buf)) >= 0)
  {
    if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ')
    {
      if (chdir(buf + 3) < 0)
        fprintf(2, "cannot cd %s\n", buf + 3);
      continue;
    }
    if (fork() == 0)
      run_cmd(cmd, parse_cmd(cmd, buf));
    wait(0);
  }

  exit(0);
}