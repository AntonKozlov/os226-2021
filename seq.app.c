#include "usyscall.h"


int atoi(const char *str) {
    int v = 0;

    int neg = 0;

    if (*str == '-') {
        str++;
        neg = 1;
    }

    while ('0' <= *str && *str <= '9') {
        v *= 10;
        v += (*(str++) - '0');
    }

    return neg ? -v : v;
}

int itoa(int v, char *d) {
    if (v < 0) {
        v = -v;
        *d = '-';
        d++;
    }

    if (!v) {
        *d = '0';
        return 1;
    }

    int v_dummy = v;
    int length = 0;

    for (; v_dummy; v_dummy /= 10) length++;

    for (int i = length - 1; i >= 0; i--) {
        d[i] = '0' + (v % 10);
        v /= 10;
    }
    return length;
}

int main(int argc, char* argv[]) {
	int n = atoi(argv[1]);
	char buf[32];
	for (int i = 1; i <= n; ++i) {
		int l = itoa(i, buf);
		buf[l] = '\n';
		os_write(1, buf, l + 1);
	}
	os_exit(0);
}
