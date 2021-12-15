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

void loop_print(char* str) {
    while (1) {
        os_print(1, str);
        for (int i = 0; i < 100000000; i++);
    }
}

int main(int argc, char* argv[]) {
	if (os_fork()) {
        if (os_fork()) loop_print("0\n");
        else loop_print("1\n");
    } else {
        if (os_fork()) loop_print("2\n");
        else loop_print("3\n");
    }

	os_print(2, "should not reach here\n");
	os_exit(1);
}
