#include <stdio.h>
#include <malloc.h>
#include <string.h>

#pragma clang diagnostic push
#pragma ide diagnostic ignored "DanglingPointer"
#define MAX_COMMAND_LINE 128
#define ERROR_CODE 0

int g_error_code = 0; // eax

typedef int (*command_func)(int argc, char *argv[]);

int echo(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
    }
    return argc - 1;
}

int retcode(int argc, char *argv[]) {
    if (printf("%d\n", g_error_code)) {
        g_error_code = 1;
        return 0;
    }

    return 1;
}

int unknown_command(int argc, char *argv[]) {
    fprintf(stderr, "Unknown command %s\n", argv[0]);
    g_error_code = ERROR_CODE;
    return ERROR_CODE;
}

command_func get_func(const char *str) {
    if (strncmp(str, "echo", 4) == 0)
        return echo;
    if (strncmp(str, "retcode", 7) == 0)
        return retcode;

    return unknown_command;
}

char *get_first_word(const char *str) {
    char *word = (char *) calloc(sizeof(char), strlen(str) + 3);

    for (int i = 0; i < (int) strlen(str); i++) {
        if (str[i] != ' ' && str[i] != '\n' && str[i] != ';' && str[i] != EOF)
            word[i] = str[i];
        else
            break;
    }

    return word;
}

char *get_body_echo(const char *str) {
    char *body = (char *) calloc(sizeof(char), (int) strlen(str));

    int count_of_spaces = 0;

    for (int i = 0; i < (int) strlen(str); i++) {
        if (str[i] != ' ') {
            body[i - count_of_spaces] = str[i];
        } else if (str[i] == ' ' && str[i + 1] != ' ' && str[i + 1] != '\0') {
            body[i - count_of_spaces] = ' ';
        } else if (str[i] == ' ' && str[i + 1] == ' ') {
            count_of_spaces++;
        }
    }

    return body;
}

int get_count_of_words(const char *str) {
    int count = 0;
    for (int i = 0; i < (int) strlen(str); i++) {
        if (str[i] == ' ' && str[i - 1] != ' ')
            count++;
    }

    return ++count;
}

char **get_argv_for_echo(const char *str, int count) {
    char **body = (char **) calloc(sizeof(char *), ++count);
    int words_len = 0;

    for (int i = 1; i < count; i++) {
        char *word = get_first_word(&str[words_len]);
        body[i] = (char *) calloc(sizeof(char), strlen(word));
        body[i] = word;

        words_len += (int) strlen(word) + 1;
    }

    return body;
}

int call_echo(const char *str) {
    char *body_of_echo = get_body_echo(&str[5]);
    int count_of_words = get_count_of_words(body_of_echo);

    char **argv_echo = get_argv_for_echo(body_of_echo, count_of_words);

    g_error_code = echo(count_of_words + 1, argv_echo);

    for (int i = 1; i < count_of_words + 1; i++) {
        free(argv_echo[i]);
    }

    free(argv_echo);
    free(body_of_echo);
    return 1;
}

int main(int argc, char *argv[]) {
    char *str = (char *) calloc(sizeof(char), MAX_COMMAND_LINE);

    for (;;) {
        fgets(str, MAX_COMMAND_LINE, stdin);
        if (feof(stdin))
            break;
        if (*str == '\n')
            continue;

        char *str_split = strtok(str, ";");

        while (str_split != NULL) {
            if (*str_split == '\n' || *str_split == ' ') {
                str_split = strtok(NULL, ";");
                continue;
            }

            char *command_name = get_first_word(str_split);
            command_func command = get_func(command_name);

            if (command == echo) {
                call_echo(str_split);
            } else if (command == retcode) {
                g_error_code = command(argc, argv);
            } else {
                command(argc, &command_name);
            }

            free(command_name);
            str_split = strtok(NULL, ";");
        }
    }

    free(str);
    return 0;
}

#pragma clang diagnostic pop