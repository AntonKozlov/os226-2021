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
	os_write(1, "start\n", 6);

	int pipe[2];
	int ret = os_pipe(pipe);
	if (ret < 0) {
		os_print(2, "cannot create pipe\n");
		os_exit(1);
	}
	int pid = os_fork();
    // fork 4 times, print indexes 1-4 from each in an endless loop
	if (pid) {
		os_close(1);
		os_dup(pipe[1]);
		const char *arg[] = { "seq", "100", (char*)0 };
		os_exec("seq", (char**)arg);
	} else {
		os_close(0);
		os_dup(pipe[0]);
		const char *arg[] = { "grep", "2", (char*)0 };
		os_exec("grep", (char**)arg);
	}

	os_print(2, "should not reach here\n");
	os_exit(1);
}
