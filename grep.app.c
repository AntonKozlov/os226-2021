#include "usyscall.h"

char *strstr(const char *haystack, const char *needle) {
    for (; *haystack != '\0'; haystack++) {
        const char *h = haystack, *n = needle;
        for (; *h != '\0' && *n != '\0' && (*h == *n); h++, n++);
        if (*n == '\0') return (char *) haystack;
    }
    return 0;
}

void *memchr(const void *str, int c, long unsigned n) {
    const unsigned char *end = (unsigned char *) (str + n);
    for (unsigned char *p = (unsigned char *) str; p < end; p++) if (*p == c) return p;
    return 0;
}

void *memmove(void *dst, const void *src, long unsigned n) {
    char buf[n], *d = dst, *s = (char *) src;
    for (long unsigned i = 0; i < n; i++) buf[i] = s[i];
    for (long unsigned i = 0; i < n; i++) d[i] = buf[i];
    return dst;
}

int main(int argc, char *argv[]) {
    char buf[5];
    int len;
    int o = 0;
    while (0 < (len = os_read(0, buf + o, sizeof(buf) - o))) {
        len += o;
        char *p = buf;
        char *p2 = memchr(p, '\n', len);
        while (p2) {
            *p2 = '\0';
            if (strstr(p, argv[1])) {
                os_write(1, p, p2 - p);
                os_write(1, "\n", 1);
            }
            p = p2 + 1;
            p2 = memchr(p, '\n', len - (p - buf));
        }
        o = len - (int) (p - buf);
        memmove(buf, p, o);
    }

    os_exit(0);
}
