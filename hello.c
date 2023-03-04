#include "types.h"                                                        
#include "stat.h"
#include "user.h"

int 
main(void) {
    printf(1, "return value of system call hello() is : %d\n", hello());
    printf(1, "System call successful!\n"); 
    exit();
}
