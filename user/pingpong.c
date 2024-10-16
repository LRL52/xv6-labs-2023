#include "kernel/types.h"
#include "user/user.h"

#define PIPE_READ 0
#define PIPE_WRITE 1

int main() {
    int pipe1[2], pipe2[2];
    char c;

    pipe(pipe1); // create pipe1 for parent to child
    pipe(pipe2); // create pipe2 for child to parent

    if (fork() == 0) {
        // child
        close(pipe1[PIPE_WRITE]);
        close(pipe2[PIPE_READ]);

        read(pipe1[PIPE_READ], &c, 1);
        printf("%d: received ping\n", getpid());
        write(pipe2[PIPE_WRITE], "c", 1);
        
        close(pipe1[PIPE_READ]);
        close(pipe2[PIPE_WRITE]);
    } else {
        // parent
        close(pipe1[PIPE_READ]);
        close(pipe2[PIPE_WRITE]);

        write(pipe1[PIPE_WRITE], "c", 1);
        read(pipe2[PIPE_READ], &c, 1);
        printf("%d: received pong\n", getpid());

        close(pipe1[PIPE_WRITE]);
        close(pipe2[PIPE_READ]);
    }

    return 0;
}