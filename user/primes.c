#include "kernel/types.h"
#include "user/user.h"

#define PIPE_READ 0
#define PIPE_WRITE 1

int main() {
    int pipe_left[2], pipe_right[2];
    int id = 0, initialized = 0;
    int p, x = 2, rc;

BEGIN:
    if (id == 0) {
        p = 2;
    } else {
        read(pipe_left[PIPE_READ], &p, 4);
    }

    printf("prime %d\n", p);
    
    while (1) {
        if (id == 0) {
            x += 1;
            if (x > 35) {
                break;
            }
        } else {
            rc = read(pipe_left[PIPE_READ], &x, 4);
            if (rc == 0) { // the write-side of the pipe has been closed
                break;
            }
        }
        if (x % p != 0) {
            if (initialized) {
                write(pipe_right[PIPE_WRITE], &x, 4);
            } else {
                pipe(pipe_right);
                if (fork() == 0) {
                    if (id > 0) {
                        close(pipe_left[PIPE_READ]);
                    }
                    memcpy(pipe_left, pipe_right, sizeof(pipe_left));
                    close(pipe_left[PIPE_WRITE]);
                    ++id;
                    goto BEGIN;
                } else {
                    close(pipe_right[PIPE_READ]);
                }
                initialized = 1;
                write(pipe_right[PIPE_WRITE], &x, 4);
            }
        }
    }

    if (id > 0) {
        close(pipe_left[PIPE_READ]);
    }
    if (initialized) {
        close(pipe_right[PIPE_WRITE]);
        wait(&rc);
    }

    return 0;
}