#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if(argc != 1){
        fprintf(2, "pingpong: illegal argument count: %d \n", argc);
        exit(1);
    }
  
    int pid;
    int fd_pipe1[2] = {0};
    int fd_pipe2[2] = {0};

    char buffer[] = {"0"};

    if(pipe(fd_pipe1) < 0 || pipe(fd_pipe2) < 0) {
         fprintf(2, "pingpong: fail to pipe()\n");
         exit(1);
    }

    pid = fork();
    if(pid < 0) {
        fprintf(2, "pingpong: fail to fork(): %d\n", pid);
        exit(1);
    } else if(pid == 0) {
        // child
        close(fd_pipe1[1]);
        close(fd_pipe2[0]);
       
        if(strlen(buffer) != read(fd_pipe1[0], buffer, strlen(buffer))) {
            fprintf(2, "pingpong: fail to read() from parents' pipe()\n");
            exit(1);
        }
        printf("%d: received ping\n", getpid());
        
        if(strlen(buffer) != write(fd_pipe2[1], buffer, strlen(buffer))) {
            fprintf(2, "pingpong: fail to write() to parents' pipe()\n");
            exit(1);
        }
        exit(0);
    } else {
        // parent
        close(fd_pipe1[0]);
        close(fd_pipe2[1]);
        
        if(strlen(buffer) != write(fd_pipe1[1], buffer, strlen(buffer))) {
            fprintf(2, "pingpong: fail to write() to child's pipe()\n");
            exit(1);
        }
        
        if(strlen(buffer) != read(fd_pipe2[0], buffer, strlen(buffer))) {
            fprintf(2, "pingpong: fail to read() from child's pipe():\n");
            exit(1);
        }
        printf("%d: received pong\n", getpid());
        exit(0);
    }

    exit(0);
}
