#include "usyscall.h"


int atoi(const char *str) {
    int res = 0;
    for (int i = 0; str[i] != '\0'; i++) res = res * 10 + str[i] - '0';
    return res;
}

int itoa(int v, char *d) {
    char buf[32];
    char *cur = buf;
    unsigned int sign = v < 0, uv;
    if (sign) uv = -v;
    else uv = v;
    while (uv > 0 || cur == buf) {
        *(cur++) = (char) (uv % 10 + '0');
        uv /= 10;
    }
    unsigned int len = cur - buf;
    if (sign) {
        *(d++) = '-';
        len++;
    }
    while (cur > buf) *(d++) = *(--cur);
    return (int) len;
}

int main(int argc, char *argv[]) {
    int n = atoi(argv[1]);
    char buf[32];
    for (int i = 1; i <= n; ++i) {
        int l = itoa(i, buf);
        buf[l] = '\n';
        os_write(1, buf, l + 1);
    }
    os_exit(0);
}
