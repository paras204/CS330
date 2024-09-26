#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{	
	if (argc < 2) {
		printf("Usage: %s <integer>\n", argv[0]);
		return 1;
	}

	int n = argc;
	int x = atoi(argv[n-1]); 
	x *= x;
	
	char str[20]; 
	sprintf(str, "%d", x); 


	argv[n-1] = str;

	if(n == 2){
		printf("%d\n", x);
		return 0;
	}
	else{
		argv++; 
		execv(argv[0], argv);
		perror("execv"); 
		return 1;
	}
	
	return 0;
}
