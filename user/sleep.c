#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    fprintf(2, "need a param");
    exit();
  }
  int i = atoi(argv[1]);
  sleep(i);
  exit();
}