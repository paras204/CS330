

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

unsigned long calculateSize_child(const char *path) {
    struct stat st;
    if (lstat(path, &st) == -1) {
        perror("lstat");
        exit(EXIT_FAILURE);
    }

    if (S_ISREG(st.st_mode)) {
        return st.st_size;
    } else if (S_ISDIR(st.st_mode)) {
        unsigned long size = 0;
        DIR *dir = opendir(path);
        if (dir == NULL) {
            perror("opendir");
            exit(EXIT_FAILURE);
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            char subpath[4096];
            snprintf(subpath, sizeof(subpath), "%s/%s", path, entry->d_name);

            unsigned long subsize = calculateSize_child(subpath);
            size += subsize;
        }

        closedir(dir);
        return size + st.st_size;
    } else if (S_ISLNK(st.st_mode)) {
        char linkedpath[4096];
        ssize_t len = readlink(path, linkedpath, sizeof(linkedpath));
        if (len == -1) {
            perror("readlink");
            exit(EXIT_FAILURE);
        }
        linkedpath[len] = '\0';

        return calculateSize_child(linkedpath);
    } else {
        return 0;
    }
}


unsigned long calculateSize(const char *path) {
    struct stat st;
    if (lstat(path, &st) == -1) {
        perror("lstat");
        exit(EXIT_FAILURE);
    }

    if (S_ISREG(st.st_mode)) {
        return st.st_size;
    } else if (S_ISDIR(st.st_mode)) {
        unsigned long size = 0;
        DIR *dir = opendir(path);
        if (dir == NULL) {
            perror("opendir");
            exit(EXIT_FAILURE);
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL ) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            char subpath[4096];
            snprintf(subpath, sizeof(subpath), "%s/%s", path, entry->d_name);

            if (S_ISLNK(entry->d_type)) {
                char linkedpath[4096];
                ssize_t len = readlink(subpath, linkedpath, sizeof(linkedpath));
                if (len == -1) {
                    perror("readlink");
                    exit(EXIT_FAILURE);
                }
                linkedpath[len] = '\0';
                size += calculateSize(linkedpath);
            } else {
                int pipefd[2];
                if (pipe(pipefd) == -1) {
                    perror("pipe");
                    exit(EXIT_FAILURE);
                }

                pid_t child_pid = fork();
                if (child_pid == -1) {
                    perror("fork");
                    exit(EXIT_FAILURE);
                }

                if (child_pid == 0) {
                    close(pipefd[0]);
                    unsigned long subsize = calculateSize_child(subpath);
                    write(pipefd[1], &subsize, sizeof(subsize));
                    close(pipefd[1]);
                    exit(EXIT_SUCCESS);
                } else {
                    close(pipefd[1]);
                    unsigned long subsize;
                    read(pipefd[0], &subsize, sizeof(subsize));
                    close(pipefd[0]);
                    size += subsize;
                    wait(NULL);
                }
            }
        }

        closedir(dir);
        return size + st.st_size;
    } else {
        return 0;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Error!", argv[0]);
        return EXIT_FAILURE;
    }

    unsigned long totalSize = calculateSize(argv[1]);
    printf("%lu\n", totalSize);

    return EXIT_SUCCESS;
}
