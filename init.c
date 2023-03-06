// init: The initial user-level program

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

char *argv[] = { "sh", 0 };

int
main(void)
{
  int pid, wpid;

  if(open("console", O_RDWR) < 0){  //Akshay: "console" is a device file, this open usually returns zero
    mknod("console", 1, 1);
    open("console", O_RDWR);
  }
  dup(0);  // stdout     //Akshay: stdout, stderr i.e. 1 and 2 will get struct file as a pointer
  dup(0);  // stderr

  for(;;){
    printf(1, "init: starting sh\n");
    pid = fork();
    if(pid < 0){
      printf(1, "init: fork failed\n");
      exit();
    }
    if(pid == 0){
      exec("sh", argv);
      printf(1, "init: exec sh failed\n");
      exit();
    }
    while((wpid=wait()) >= 0 && wpid != pid)    //Akshay: only when the parent has called wait, can the PCB of child be allocated to another process
      printf(1, "zombie!\n");
  }
}
