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
	int num = 100000000;
	char str[3];
	if (os_fork()) {
		if (os_fork()) {
			strcpy(str, "0\n");
			while (1) {
				os_print(1, str);
				for (int i = 0; i < num; i++) {  }
			}
		}
		else {
			strcpy(str, "1\n");
			while (1) {
				os_print(1, str);
				for (int i = 0; i < num; i++) {  }
			}
		}
	}

	else {
		if (os_fork()) {
			strcpy(str, "2\n");
			while (1) {
				os_print(1, str);
				for (int i = 0; i < num; i++) {  }
			}
		}
		else {
			strcpy(str, "3\n");
			while (1) {
				os_print(1, str);
				for (int i = 0; i < num; i++) {  }
			}
		}
	}

	os_print(2, "should not reach here\n");
	os_exit(1);
}
