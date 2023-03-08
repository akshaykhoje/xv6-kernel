#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

int
main(int argc, char *argv[]) {
  int fp;
  char *buffer = 0;
  uint len;
  if (argc != 5) {
    printf(1, "Syntax: ./lseek1 filename offset len string \n");
    exit();
  }

  if ((fp = open(argv[1], O_RDONLY)) < 0) {
    printf(1, "FILE NOT FOUND!%s\n", argv[1]);
    exit();
  }
  
  len = atoi(argv[3]);
  if ((buffer = (char *)malloc(len + 1)) < 0) {
    printf(1, "NOT ENOUGH SPACE FOR BUFFER\n");
    exit();
  }

  int offset = atoi(argv[2]);
  int ret;
  ret = lseek(fp, SEEK_SET, offset);
  if (ret < 0) {
    printf(1, "unable to lseek\n");
    exit();
  }

  read(fp, buffer, len);
  buffer[len] = 0;

  printf(1, "%s : %s\n", argv[4], buffer);

  if (strcmp(buffer, argv[4])) {
    printf(1, "Different string\n");
    exit();
  }
  printf(1, "Strings match\n");

  exit();
}
