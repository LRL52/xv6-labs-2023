#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define STDIN 0
#define BUF_SIZE 512

int main(int argc, char *argv[]) {
    char *new_argv[MAXARG];
    char buf[BUF_SIZE];
    int i, n, rc, new_argc;

    if (argc < 2) {
        fprintf(2, "usage: xargs command\n");
        exit(1);
    }

    new_argc = argc - 1;
    for (i = 0; i < new_argc; ++i) {
        new_argv[i] = argv[i + 1];
    }

    while (1) {
        n = 0;
        while ((rc = read(STDIN, buf + n, 1)) > 0) { // read one character at a time until newline('\n')
            if (buf[n] == '\n') {
                break;
            }
            ++n;
        }
        if (rc == 0) {
            break;
        }
        buf[n] = 0;
        new_argv[new_argc] = buf;
        new_argv[new_argc + 1] = 0; // argv[] must be terminated by NULL
        if (fork() == 0) {
            exec(argv[1], new_argv);
            exit(1);
        }
        wait(0);
    }


    return 0;
}