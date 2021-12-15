#include <stdbool.h>
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
	unsigned int id = (os_fork() == 0 ? 0 : 2) + (os_fork() == 0 ? 0 : 1);
	char str[] = {id + '0', '\n', '\0'};
	while (true) {
		os_print(1, str);
		for (int i = 0; i < 100000000; i++);
	}
}
