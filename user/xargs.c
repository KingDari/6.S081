#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[]) {
	char *argvNex[MAXARG];
	for(int i = 1; i < argc; i++) {
		// printf("%s\n", argv[i]);
		argvNex[i-1] = argv[i];
	}
	char buf[32];
	char *pos = buf;
	while(read(0, pos, 1)) {
		if(*pos != '\n') {  
			pos++;
		} else {
			*pos = 0;
			// printf("%s\n", buf);
			if(fork() == 0) {
				// child
				argvNex[argc - 1] = buf;
				exec(argvNex[0], argvNex);
				fprintf(2, "xargs: fail to exec()\n");
				exit(1);
			} else {
				wait(0);
			}
			pos = buf;
		}	
	}
	exit(0);
}
