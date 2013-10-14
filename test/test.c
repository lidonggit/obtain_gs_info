#include <stdio.h>


union u_tag{ 
  char words[1000];
  int m;
} code;
main() 
{
   code.m=1;
   printf("m=%d\n", code.m);
 
}  
