#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <integer>\n", argv[0]);
        return 1;
    }

    int n = argc; 
    int x = atoi(argv[n - 1]);
    x *= 2;
    sprintf(argv[n - 1], "%d", x); 

    if (n == 2) {
        printf("%d\n", x);
        return 0;
    } else {
		argv++; 
        execv(argv[0], argv);
        perror("execv"); 
        return 1;
    }

    return 0;
}
