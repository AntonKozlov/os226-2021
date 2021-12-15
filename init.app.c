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

void wait() { for(int i = 0; i < 100000000; i++); }

int main(int argc, char* argv[]) {
    int pid1 = os_fork();
    if(pid1 == 0) {
        int pid2 = os_fork();
        if(pid2 == 0) {
            while(1) {
                os_print(1, "0 pid\n");
                wait();
            }
        }
        else {
            while(1) {
                os_print(1, "1 pid\n");
                wait();
            }
        }
    }
    else {
        int pid2 = os_fork();
        if(pid2 == 0) {
            while(1) {
                os_print(1, "2 pid\n");
                wait();
            }
        }
        else {
            while(1) {
                os_print(1, "3 pid\n");
                wait();
            }
        }
    }
}
