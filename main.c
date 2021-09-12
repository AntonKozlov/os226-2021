
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT_STRING_LENGTH 1023
#define COMMAND_DELIM ";\n"
#define TOKEN_DELIM " "

int code = 0;

int echo(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ') < 0) return -1;
    }
    return argc - 1;
}

int ret_code(int argc, char* argv[]) {
    if (printf("%d\n", code) >= 0) return 0;
    else return -1;
}

int parse_tokens(char* cmd, char*** tokens, int* n_t) {
    *tokens = NULL;
    *n_t = 0;
    char* tkn = strtok(cmd, TOKEN_DELIM);
    while (tkn) {
        *tokens = realloc(*tokens, sizeof(char*) * ++(*n_t));
        (*tokens)[(*n_t) - 1] = tkn;
        tkn = strtok(NULL, TOKEN_DELIM);
    }
    return 0;
}

int exec_commands(char** tokens, const int n_t) {
    char* cmd_name = tokens[0];
    if (strcmp(cmd_name, "echo") == 0)
        code = echo(n_t, tokens);
    else if (strcmp(cmd_name, "retcode") == 0)
        code = ret_code(n_t, tokens);
    return 0;
}

int parse_commands(char* buf, char*** commands, int* n_c) {
    *commands = NULL;
    *n_c = 0;
    char* cmd = strtok(buf, COMMAND_DELIM);
    while (cmd) {
        *commands = realloc(*commands, sizeof(char*) * ++(*n_c));
        (*commands)[(*n_c) - 1] = cmd;
        cmd = strtok(NULL, COMMAND_DELIM);
    }
    return 0;
}

int main(int argc, char* argv[]) {
    char buf[MAX_INPUT_STRING_LENGTH];
    char** commands;
    int n_c;
    while (fgets(buf, MAX_INPUT_STRING_LENGTH, stdin) != NULL) {
        parse_commands(buf, &commands, &n_c);

        for (int i = 0; i < n_c; i++) {
            char** tokens;
            int n_t;
            parse_tokens(commands[i], &tokens, &n_t);
            exec_commands(tokens, n_t);
        }
    }
    return 0;
}

