#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void primes(int leftPipe) {
	int buffer;
	int prime;
	int p[2];
	pipe(p);
	if(read(leftPipe, &prime, sizeof(int))) {
		printf("prime %d\n", prime);
	} else {
		close(leftPipe);
		close(p[0]);
		close(p[1]);
		exit(0);
	}
	int pid = fork();
	if(pid < 0) {
		fprintf(2, "fail to fork()\n");
		exit(1);
	} else if(pid == 0) {
		// child
		close(leftPipe);
		close(p[1]);
		primes(p[0]);
		exit(0);
	} else {
		// parent filter
		close(p[0]);
		while(read(leftPipe, &buffer, sizeof(int))) {
			if(buffer % prime != 0) {
				write(p[1], &buffer, sizeof(int));
			}
		}
		close(leftPipe);
		close(p[1]);
		wait(&pid);
		exit(0);
	}
}

int main(int argc, char *argv[]) {
    if(argc != 1){
        fprintf(2, "primes: illegal argument count: %d \n", argc);
        exit(1);
    }
	
	int p[2];
	pipe(p);
	for(int i = 2; i < 36; i++) {
		write(p[1], &i, sizeof(int));
	}
	close(p[1]);
	primes(p[0]);

    exit(0);
}
