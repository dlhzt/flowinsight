#include "license.h"
int main()   
{ 
	char buff[2048];
	int i;
	for(i=0;i<2048;i++)
	  buff[i]=(i<<2)&0x45f7;  
 	printf("Register ID: %8lX\n", CreateLock());
 	return 0;
} 
