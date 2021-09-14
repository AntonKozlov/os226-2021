#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LENGTH 128

int returned = 0;

int echo(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
    }
    return argc - 1;
}

int retcode(int argc, char *argv[]) {
    printf("%d\n", returned);
    return 0;
}

void command_parser(char **commands, int cnt) {
    for (int i = 0; i < cnt; i++) {
        char **words = NULL;
        char *word = strtok(commands[i], " ");
        int words_cnt = 0;
        while (word != NULL) {
            words = realloc(words, (words_cnt + 1) * sizeof(char *));
            words[words_cnt] = word;
            word = strtok(NULL, " ");
            words_cnt++;
        }
        if (strcmp(words[0], "echo") == 0) {
            returned = echo(words_cnt, words);
        } else if (strcmp(words[0], "retcode") == 0) {
            returned = retcode(words_cnt, words);
        } else returned = -1;
        free(words);
    }
}

int stdin_parser() {
    while (1) {
        char string[MAX_LENGTH] = "";
        if (fgets(string, MAX_LENGTH, stdin) == NULL) {
            return 0;
        }
        char **buffer = NULL;
        char *command = strtok(string, ";\n");
        int cnt = 0;
        while (command != NULL) {
            buffer = realloc(buffer, (cnt + 1) * sizeof(char *));
            buffer[cnt] = command;
            cnt++;
            command = strtok(NULL, ";\n");
        }
        command_parser(buffer, cnt);
        free(buffer);
    }
}

int main(int argc, char *argv[]) {
    stdin_parser();

    return 0;
}
