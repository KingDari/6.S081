#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 2){
    fprintf(2, "sleep: illegal argument count: %d \n", argc);
    exit(1);
  }
  
  int time = atoi(argv[1]);
  printf("sleep: sleep time %d \n", time);

  sleep(time);

  exit(0);
}
