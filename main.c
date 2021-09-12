#define _POSIX_C_SOURCE 1

#include <string.h>
#include <stdio.h>

#define MAX_LINE_LENGTH 1024
#define MAX_ARGC 1024
#define COMMAND_DELIM ";\n"
#define ARG_DELIM " "

int last_retcode = 0;

int echo(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
    }
    return argc - 1;
}

int retcode(int argc, char *argv[]) {
    printf("%d\n", last_retcode);
    return 0;
}

typedef int (*command_func_t)(int argc, char *argv[]);

typedef struct {
    char *name;
    command_func_t func;
} command_t;

command_t commands[] = {
        {.name = "echo", .func = echo},
        {.name = "retcode", .func = retcode}
};

size_t command_count = sizeof(commands) / sizeof(command_t);

void run_command(char *command_str) {
    int argc = 0;
    char *argv[MAX_ARGC];
    char *strtok_saveptr;
    char *arg = strtok_r(command_str, ARG_DELIM, &strtok_saveptr);
    while (arg != NULL) {
        argv[argc++] = arg;
        arg = strtok_r(NULL, ARG_DELIM, &strtok_saveptr);
    }
    char *command_name = argv[0];
    for (size_t i = 0; i < command_count; i++)
        if (strcmp(command_name, commands[i].name) == 0) {
            last_retcode = commands[i].func(argc, argv);
            return;
        }
    fprintf(stderr, "Unknown command: %s\n", command_name);
}

int main(int argc, char *argv[]) {
    char line[MAX_LINE_LENGTH];
    while (fgets(line, MAX_LINE_LENGTH, stdin) != NULL) {
        char *strtok_saveptr;
        char *command_str = strtok_r(line, COMMAND_DELIM, &strtok_saveptr);
        while (command_str != NULL) {
            run_command(command_str);
            command_str = strtok_r(NULL, COMMAND_DELIM, &strtok_saveptr);
        }
    }
    return 0;
}
