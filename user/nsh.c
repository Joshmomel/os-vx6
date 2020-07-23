#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define MAX_CMD 8
#define MAX_PIPE (MAX_CMD - 1) * 2
#define MAXARGS 10

void test(char *buf);
char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";
char whitespace_and_symbols[] = " \t\r\n\v<|>&;()";

struct cmd
{
  char *args[MAXARGS];
  char argc;
  char *input;
  char *output;
};

void init_cmd(struct cmd *cmd, int index);
void print_cmd(struct cmd *cmd, int size);
int skip(char *token, const char *given_string);

int parsecmd(struct cmd *cmd, char *buf)
{
  int read_cmd_size = 1;

  char *p = buf;
  while (*p)
  {

    int cmd_index = read_cmd_size - 1;
    while (*p && *p == ' ')
      *p++ = 0;

    init_cmd(cmd, cmd_index);
    while (*p && skip(whitespace_and_symbols, p))
    {
      // printf("get the value->%s\n", p);
      // printf("argc is %d\n", cmd[cmd_index].argc);
      cmd[cmd_index].argc++;
      cmd[cmd_index].args[cmd[cmd_index].argc - 1] = p;
      while (*p && skip(whitespace_and_symbols, p))
        p++;
      while (*p && *p == ' ')
        *p++ = 0;
    }
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
  // print_cmd(cmd, read_cmd_size);
  // printf("read cmd size is %d\n", read_cmd_size);
  return read_cmd_size;
}

void redirect(int k, int pd[])
{
  close(k);
  dup(pd[k]);
  close(pd[0]);
  close(pd[1]);
}

void handle(struct cmd *cmd)
{
  // fprintf(2, "handle %d is called\n", getpid());
  if (cmd[0].input)
  {
    // fprintf(2, "input is %s\n", cmd[0].input);
    close(0);
    open(cmd[0].input, O_RDONLY);
  }
  if (cmd[0].output)
  {
    // fprintf(2, "input is %s\n", cmd[0].output);
    close(1);
    open(cmd[0].output, O_WRONLY | O_CREATE);
  }
  // fprintf(2, "handle %d is done\n", getpid());
  exec(cmd[0].args[0], cmd[0].args);
  exit(0);
}

void run_cmd(struct cmd *cmd, int cmd_size)
{
  if (cmd_size)
  {
    // fprintf(2, "cmd size is %d\n", cmd_size);
    int pd[2];
    pipe(pd);
    // int parent_pid = getpid();
    // fprintf(2, "pid: %d\n", parent_pid);
    if (fork() > 0)
    {
      // fprintf(2, "%d -> %d source\n", parent_pid, getpid());
      if (cmd_size > 1)
      {
        // fprintf(2, "%d redirect\n", getpid());
        redirect(1, pd);
      }
      handle(cmd);
    }
    else if (fork() > 0)
    {
      // fprintf(2, "%d -> %d sink\n", parent_pid, getpid());
      if (cmd_size > 1)
      {
        // fprintf(2, "%d redirect\n", getpid());
        redirect(0, pd);
        cmd_size = cmd_size - 1;
        cmd = cmd + 1;
        // print_cmd(cmd, cmd_size);
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

void init_cmd(struct cmd *cmd, int index)
{
  cmd[index].argc = 0;
  for (int i = 0; i < MAXARGS; i++)
    cmd[index].args[i] = 0;
}

void print_cmd(struct cmd *cmd, int cmd_sz)
{
  // for (int i = 0; i < read_cmd_size; i++)
  // {
  //   for (int j = 0; j < cmd[i].argc; j++)
  //   {
  //     printf("the %d cmd's %d argument is %s\n", i, j, cmd[i].args[j]);
  //   }
  // }
  for (int i = 0; i < cmd_sz; i++)
  {
    printf("cmd size in the print is %d\n", cmd_sz);
    printf("cmd[%d].args", i);
    for (int j = 0; j < cmd[i].argc; j++)
      printf(" %s(%d)", cmd[i].args[j], strlen(cmd[i].args[j]));
    printf("\n");
    printf("cmd[%d].argc %d\n", i, cmd[i].argc);
    printf("cmd[%d].input %s\n", i, cmd[i].input);
    printf("cmd[%d].output %s\n", i, cmd[i].output);
  }
}

int skip(char *token, const char *given_string)
{
  return !strchr(whitespace_and_symbols, *given_string);
}

int getcmd(char *buf, int nbuf)
{
  write(1, "@ ", strlen("@ "));
  memset(buf, 0, 1024);
  gets(buf, 1024);

  if (buf[0] == 0) // EOF
    exit(0);

  *strchr(buf, '\n') = '\0';
  return 0;
}

int main(int argc, char *argv[])
{
  static char buf[100];
  static struct cmd cmd[MAX_CMD];

  while (getcmd(buf, sizeof(buf)) >= 0)
  {
    // printf("buf in main is %s\n", buf);
    // test(buf);
    if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ')
    {
      // Chdir must be called by the parent, not the child.
      // printf("strlen of buf is %d\n", strlen(buf));
      if (chdir(buf + 3) < 0)
        fprintf(2, "cannot cd %s\n", buf + 3);
      continue;
    }
    if (fork() == 0)
      run_cmd(cmd, parsecmd(cmd, buf));
    wait(0);
  }

  exit(0);
}