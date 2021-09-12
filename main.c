
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT_STRING_LENGTH 1024
#define COMMAND_DELIM ";\n"
#define TOKEN_DELIM " "

enum {
    OK,
    ERROR = -1
};

int return_code = OK;

//------------------------------------------------------------------------------------------------------------- COMMANDS

#define ECHO "echo"
#define RETCODE "retcode"

//--------------------------------------------------------------------------------------------------------- DECLARATIONS

int echo(int argc, char* argv[]);

int ret_code(int argc, char* argv[]);

int exec_commands(char** tokens, int tnk_num);

int parse_with_delim(char* buf, char*** commands, int* cmd_num, const char* delim);

//----------------------------------------------------------------------------------------------------------------------

int echo(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ') < 0)
            return ERROR;
    }
    return argc - 1;
}

int ret_code(int argc, char* argv[]) {
    if (printf("%d\n", return_code) < 0)
        return ERROR;
    else return OK;
}

int exec_commands(char** tokens, const int tnk_num) {
    char* cmd_name = tokens[0];
    if (strcmp(cmd_name, ECHO) == 0)
        return_code = echo(tnk_num, tokens);
    else if (strcmp(cmd_name, RETCODE) == 0)
        return_code = ret_code(tnk_num, tokens);
    else if (printf("%s: command not found\n", cmd_name) < 0)
        return ERROR;
    return OK;
}

int parse_with_delim(char* buf, char*** commands, int* cmd_num, const char* delim) {
    *commands = NULL;
    *cmd_num = 0;
    char* cmd = strtok(buf, delim);
    while (cmd) {
        *commands = realloc(*commands, sizeof(char*) * ++(*cmd_num));
        if (!*commands)
            return ERROR;
        (*commands)[(*cmd_num) - 1] = cmd;
        cmd = strtok(NULL, delim);
    }
    return OK;
}

//----------------------------------------------------------------------------------------------------------------- MAIN

int main(int argc, char* argv[]) {
    char buf[MAX_INPUT_STRING_LENGTH];
    while (fgets(buf, MAX_INPUT_STRING_LENGTH, stdin) != NULL) {
        char** commands;
        int cmd_num;
        if (parse_with_delim(buf, &commands, &cmd_num, COMMAND_DELIM) != 0) {
            free(commands);
            return ERROR;
        }
        for (int i = 0; i < cmd_num; i++) {
            char** tokens;
            int tnk_num;
            if (parse_with_delim(commands[i], &tokens, &tnk_num, TOKEN_DELIM) != 0 ||
                exec_commands(tokens, tnk_num) != 0) {
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
