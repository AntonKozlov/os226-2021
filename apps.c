#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "usyscall.h"
#include "pool.h"

// DEFINITIONS

#define MAX_INPUT_LINE_LEN 1024
#define COMMAND_DELIM ";\n"
#define TOKEN_DELIM " "

// GLOBALS

enum RETURN_CODES {
    OK, ERROR
};

int last_return_code = OK;

// APPLICATION DECLARATIONS

#define APPS_X(X) \
        X(echo) \
        X(retcode) \
        X(pooltest)\
        X(syscalltest) \

#define DECLARE(X) static int X(int, char* []);

APPS_X(DECLARE)

#undef DECLARE

static const struct app {
    const char* name;

    int (* fn)(int, char* []);
} app_list[] = {
#define ELEM(X) { #X, X },
        APPS_X(ELEM)
#undef ELEM
};

// APPLICATIONS

static int os_printf(const char* fmt, ...) {
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return os_print(buf, ret);
}

static int echo(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
    }
    fflush(stdout);
    return argc - 1;
}

static int retcode(int argc, char* argv[]) {
    printf("%d\n", last_return_code);
    fflush(stdout);
    return OK;
}

static int pooltest(int argc, char* argv[]) {
    struct obj {
        void* field1;
        void* field2;
    };

    static struct obj objmem[4];
    static struct pool objpool = POOL_INITIALIZER_ARRAY(objmem);

    if (!strcmp(argv[1], "alloc")) {
        struct obj* o = pool_alloc(&objpool);
        printf("alloc %ld\n", o ? (o - objmem) : -1);
    } else if (!strcmp(argv[1], "free")) {
        int iobj = (int) strtol(argv[2], NULL, 10);
        printf("free %d\n", iobj);
        pool_free(&objpool, objmem + iobj);
    } else {
        fprintf(stderr, "Unknown argument for pooltest: %s\n", argv[1]);
    }

    return OK;
}

static int syscalltest(int argc, char* argv[]) {
    int r = os_printf("%s\n", argv[1]);
    return r - 1;
}

// HELPER FUNCTIONS

int divide_str(char* source, char* delim, char*** container, int* container_size) {
    *container = NULL;
    *container_size = 0;

    char* token = strtok(source, delim);
    while (token != NULL) {
        *container = realloc(*container, ++(*container_size) * sizeof(char*));
        if (*container == NULL) {
            fprintf(stderr, "String division failed at token %d\n", *container_size);
            return ERROR;
        }
        (*container)[(*container_size) - 1] = token;
        token = strtok(NULL, delim);
    }

    return OK;
}

int execute(int tokens_num, char** tokens) {
    if (tokens_num <= 0) return OK;

    for (int i = 0; i < ARRAY_SIZE(app_list); i++) {
        if (strcmp(tokens[0], app_list[i].name) == 0) {
            last_return_code = app_list[i].fn(tokens_num, tokens);
            return OK;
        }
    }

    if (printf("Unknown command: %s\n", tokens[0]) < 0) {
        fprintf(stderr, "Cannot print to stdin\n");
        return ERROR;
    }

    return OK;
}

// SHELL

int shell(int argc, char* argv[]) {
    char line[MAX_INPUT_LINE_LEN];

    while (fgets(line, MAX_INPUT_LINE_LEN, stdin) != NULL) {
        char** commands;
        int command_num;

        if (divide_str(line, COMMAND_DELIM, &commands, &command_num) != OK) {
            free(commands);
            return ERROR;
        }

        for (int i = 0; i < command_num; i++) {
            char** tokens;
            int token_num;

            if (divide_str(commands[i], TOKEN_DELIM, &tokens, &token_num) != OK ||
                execute(token_num, tokens) != OK) {
                free(tokens);
                free(commands);
                return ERROR;
            }

            free(tokens);
        }

        free(commands);
    }

    return OK;
}
