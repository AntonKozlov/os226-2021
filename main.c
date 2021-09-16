#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// DEFINITIONS

#define MAX_INPUT_LINE_LEN 1024
#define COMMAND_DELIM ";\n"
#define TOKEN_DELIM " "

// COMMANDS

#define ECHO "echo"
#define RETCODE "retcode"

// GLOBALS

enum RETURN_CODES {
    OK, ERROR
};

int last_return_code = OK;

// EXECUTABLES

int echo(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
    }
    return argc - 1;
}

int retcode(int argc, char* argv[]) {
    printf("%d\n", last_return_code);
    return OK;
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

    if (strcmp(tokens[0], ECHO) == 0) {
        last_return_code = echo(tokens_num, tokens);
    } else if (strcmp(tokens[0], RETCODE) == 0) {
        last_return_code = retcode(tokens_num, tokens);
    } else if (printf("Unknown command: %s\n", tokens[0])) {
        fprintf(stderr, "Cannot print to stdin\n");
        return ERROR;
    }

    return OK;
}

// MAIN

int main(int argc, char* argv[]) {
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
