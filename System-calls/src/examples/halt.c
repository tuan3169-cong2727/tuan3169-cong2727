/* halt.c

   Simple program to test whether running a user program works.
 	
   Just invokes a system call that shuts down the OS. */

#include <syscall.h>
#include "../lib/stdio.h"

int main(void)
{
    printf("halt start\n");
    //halt();
    printf("halt succes\n");
    /* not reached */
}
