#include <stdio.h>
#include <string.h>
#include <malloc.h>

#define MAX_INPUT_LENGTH 1024
#define MAX_TOKENS_IN_INPUT 64
#define UNEXPECTED_COMMAND_ERROR -1

int ret_code;


void free_string_array(char **arr, int arr_length) {
    for (int i = 0; i < arr_length; ++i) {
        if (strlen(arr[i]) > 0) {
            free(arr[i]);
        }
    }
    free(arr);
}


int retcode(int argc, char *argv[]) {
    printf("%d\n", ret_code);
    return 0;
}


int echo(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
    }
    return argc - 1;
}


void split_by(char *delim,
              char *source,
              char **base,
              int *count_of_pieces) {

    char *s_copy = malloc(strlen(source));
    strcpy(s_copy, source);

    char *token = strtok(s_copy, delim);
    int splitted_pieces = 0;

    while (token != NULL) {
        base[splitted_pieces] = malloc(strlen(token));
        strcpy(base[splitted_pieces++], token);
        token = strtok(NULL, delim);
    }

    *count_of_pieces = splitted_pieces;
    free(s_copy);
}


int process_query(int commands_in_query, char *commands[]) {
    char *command = commands[0];
    if (!strcmp(command, "echo")) {
        return echo(commands_in_query, commands);
    } else if (!strcmp(command, "retcode")) {
        return retcode(commands_in_query, commands);
    } else {
        return UNEXPECTED_COMMAND_ERROR;
    }
}


void trim_last_newline_symbol(char *s) {
    size_t len = strlen(s);
    if (s[len - 1] == '\n') {
        s[len - 1] = '\0';
    }
}


int process_input() {
    char input[MAX_INPUT_LENGTH];
    while (fgets(input, MAX_INPUT_LENGTH, stdin)) {
        trim_last_newline_symbol(input);
        int count_of_queries = 0;
        char **queries = malloc(MAX_TOKENS_IN_INPUT);
        split_by(";", input, queries, &count_of_queries);
        for (int i = 0; i < count_of_queries; ++i) {
            int commands_in_query = 0;
            char **commands = malloc(MAX_TOKENS_IN_INPUT);
            split_by(" ", queries[i], commands, &commands_in_query);
            if (*commands != NULL) {
                ret_code = process_query(commands_in_query, commands);
            }
            free_string_array(commands, commands_in_query);
        }
        free_string_array(queries, count_of_queries);
    }
    return 0;
}


int main(int argc, char *argv[]) {
    return process_input();
}
