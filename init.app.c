#include "usyscall.h"

long unsigned strlen(const char *str) {
    const char *e = str;
    while (*e) {
        ++e;
    }
    return e - str;
}

int os_print(int fd, const char *str) {
    int len = strlen(str);
    return os_write(fd, str, len);
}

int main(int argc, char* argv[]) {
    int wait_time = 100000000;
    int pid1 = os_fork();
    if(pid1 == 0) {
        int pid2 = os_fork();
        if(pid2 == 0) {
            while(1) {
                os_write(1, "0 pid\n", 6);
                for(int i = 0; i < wait_time; i++);
            }
        }
        while(1) {
            os_write(1, "1 pid\n", 6);
            for(int i = 0; i < wait_time; i++);
        }
    }
    int pid2 = os_fork();
    if(pid2 == 0) {
        while(1) {
            os_write(1, "2 pid\n", 6);
            for(int i = 0; i < wait_time; i++);
        }
    }
    while(1) {
        os_write(1, "3 pid\n", 6);
        for(int i = 0; i < wait_time; i++);
    }
    os_exit(0);
}