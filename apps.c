#define _GNU_SOURCE

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "sched.h"
#include "usyscall.h"
#include "pool.h"

#define MAX_INPUT_STRING_LENGTH 1024
#define COMMAND_DELIM ";\n"
#define TOKEN_DELIM " \t"

enum {
    OK,
    NOT_FOUND_COMMAND_ERROR = 127,
    ERROR = -1
};

static int return_code = OK;

//---------------------------------------------------------------------------------------------------------------- APPS

#define APPS_X(X) \
        X(echo) \
        X(retcode) \
        X(pooltest) \
        X(syscalltest) \
        X(app) \
        X(sched) \

#define DECLARE(X) static int X(int, char *[]);

APPS_X(DECLARE)

#undef DECLARE

static const struct app {
    const char *name;

    int (*fn)(int, char *[]);
} app_list[] = {
#define ELEM(X) { # X, X },
        APPS_X(ELEM)
#undef ELEM
};

//-------------------------------------------------------------------------------------------------------- DECLARATIONS

int exec_commands(int, char **);

int parse_with_delim(char *, char ***, int *, const char *);

//----------------------------------------------------------------------------------------------------------------------

int echo(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ') < 0)
            return ERROR;
    }
    fflush(stdout);
    return argc - 1;
}

int retcode(int argc, char *argv[]) {
    if (printf("%d\n", return_code) < 0)
        return ERROR;
    fflush(stdout);
    return OK;
}

static long reftime(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int os_printf(const char *fmt, ...) {
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return os_print(buf, ret);
}

//------------------------------------------------------------------------------------------------------ TASK SCHEDULER

struct app_ctx {
    int cnt;
} ctxarray[16];

struct pool ctxpool = POOL_INITIALIZER_ARRAY(ctxarray);

static void print(struct app_ctx *ctx, const char *msg) {
    static long refstart;
    if (!refstart) {
        refstart = reftime();
    }

    printf("%16s id %ld cnt %d time %ld reftime %ld\n",
           msg, 1 + ctx - ctxarray, ctx->cnt, sched_gettime(), reftime() - refstart);
    fflush(stdout);
}

static void app_burn(void *_ctx) {
    struct app_ctx *ctx = _ctx;
    while (1) {
        print(ctx, "burn");
        for (volatile int i = 100000 * ctx->cnt; 0 < i; --i) {
        }
    }
}

static void app_preempt_sleep(void *_ctx) {
    struct app_ctx *ctx = _ctx;
    while (1) {
        print(ctx, "sleep");
        sched_sleep(ctx->cnt);
    }
}

static int app(int argc, char *argv[]) {
    int entry_id = (int) strtol(argv[1], NULL, 10) - 1;

    struct app_ctx *ctx = pool_alloc(&ctxpool);
    ctx->cnt = (int) strtol(argv[2], NULL, 10);

    void (*entries[])(void *) = {app_burn, app_preempt_sleep};
    sched_new(entries[entry_id], ctx, (int) strtol(argv[3], NULL, 10));
    return OK;
}

static int sched(int argc, char *argv[]) {
    sched_run((int) strtol(argv[1], NULL, 10));
    return OK;
}

//--------------------------------------------------------------------------------------------------------------- TESTS

static int pooltest(int argc, char *argv[]) {
    struct obj {
        void *field1;
        void *field2;
    };
    static struct obj objmem[4];
    static struct pool objpool = POOL_INITIALIZER_ARRAY(objmem);

    if (!strcmp(argv[1], "alloc")) {
        struct obj *o = pool_alloc(&objpool);
        printf("alloc %ld\n", o ? (o - objmem) : -1);
        return OK;
    } else if (!strcmp(argv[1], "free")) {
        int iobj = (int) strtol(argv[2], NULL, 10);
        printf("free %d\n", iobj);
        pool_free(&objpool, objmem + iobj);
        return OK;
    }
    return OK;
}

static int syscalltest(int argc, char *argv[]) {
    int r = os_printf("%s\n", argv[1]);
    return r - 1;
}

//---------------------------------------------------------------------------------------------------------------- MAIN

int exec_commands(const int token_num, char **tokens) {
    char *cmd_name = tokens[0];
    const struct app *app = NULL;
    for (int i = 0; i < ARRAY_SIZE(app_list); ++i) {
        if (!strcmp(cmd_name, app_list[i].name)) {
            app = &app_list[i];
            break;
        }
    }

    if (!app) {
        if (printf("%s: command not found\n", cmd_name) < 0)
            return ERROR;
        return_code = NOT_FOUND_COMMAND_ERROR;
        return OK;
    }

    return_code = app->fn(token_num, tokens);
    return OK;
}

int parse_with_delim(char *buf, char ***container, int *container_size, const char *delim) {
    *container = NULL;
    *container_size = 0;
    char *cmd = strtok(buf, delim);
    while (cmd && cmd[0] != '#') {
        *container = realloc(*container, sizeof(char *) * ++(*container_size));
        if (!*container)
            return ERROR;
        (*container)[(*container_size) - 1] = cmd;
        cmd = strtok(NULL, delim);
    }
    return OK;
}

int shell(int argc, char *argv[]) {
    char buf[MAX_INPUT_STRING_LENGTH];
    while (fgets(buf, MAX_INPUT_STRING_LENGTH, stdin) != NULL) {
        char **commands;
        int command_num;
        if (parse_with_delim(buf, &commands, &command_num, COMMAND_DELIM) != 0) {
            free(commands);
            return ERROR;
        }
        for (int i = 0; i < command_num; i++) {
            char **tokens;
            int token_num;
            if (parse_with_delim(commands[i], &tokens, &token_num, TOKEN_DELIM) != 0 ||
                exec_commands(token_num, tokens) != 0) {
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
