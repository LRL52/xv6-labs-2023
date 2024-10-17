#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define BUF_SIZE 512

char *get_filename(char *path) {
    char *p;

    for (p = path + strlen(path); p >= path && *p != '/'; --p);
    ++p;

    return p;
}

void find(char *path, char *filename) {
    int fd;
    struct dirent de;
    struct stat st;
    char *p;

    if ((fd = open(path, O_RDONLY)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type) {
        case T_FILE:
            if (strcmp(get_filename(path), filename) == 0) {
                printf("%s\n", path);
            }
            break;

        case T_DIR:
            if (strlen(path) + 1 + DIRSIZ + 1 > BUF_SIZE) { // "1" for "/" and "1" for null terminator
                printf("find: path too long\n");
                break;
            }
            p = path + strlen(path);
            *p++ = '/'; // p points to position after '/' and will not change in the following loop
            while (read(fd, &de, sizeof(de)) == sizeof(de)) {
                if (de.inum == 0) {
                    continue;
                }
                if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
                    continue;
                }
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;
                if (stat(path, &st) < 0) {
                    fprintf(2, "find: cannot stat %s\n", path);
                    continue;
                }
                find(path, filename);
            }
            break;
    }

    close(fd);
}

int main(int argc, char *argv[]) {
    static char buf[BUF_SIZE];

    if (argc != 3) {
        fprintf(2, "usage: find <path> <filename>\n");
        exit(1);
    }

    strcpy(buf, argv[1]);
    find(buf, argv[2]);

    return 0;
}