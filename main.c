#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define STRING_PIECE 128
#define UNKNOWN_COMMAND -1024

int return_code;

int echo(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
    }
    return argc - 1;
}

int retcode(int argc, char *argv[]) {
    printf("%d\n", return_code);
    return 0;
}

int cmd_executor(char **words, int words_len) {
    char *cmd = words[0];
    if (!strcmp(cmd, "echo")) {
        return echo(words_len, words);
    } else
    if (!strcmp(cmd, "retcode")) {
        return retcode(words_len, words);
    } else {
        return UNKNOWN_COMMAND;
    }
}

void cmd_parser() {
    char buf[STRING_PIECE];
    while (1) {
        char *line;
        if (strlen(buf) > 0) {
            line = malloc(strlen(buf));
            strcpy(line, buf);
            strcpy(buf, "");
        } else {
            line = NULL;
            do {
                if (fgets(buf, STRING_PIECE, stdin) == NULL) break;
                line = realloc(line, sizeof(line) + strlen(buf));
                strcat(line, buf);
            } while (strlen(buf) == STRING_PIECE - 1 && buf[STRING_PIECE - 1] != '\n');
        }
        strcpy(buf, "");
        if (line == NULL) continue;
        unsigned long temp = strlen(line) - 1;
        if (line[temp] == '\n')  line[temp] = '\0';
        if (!strcmp(line, "")) continue;
        char *next_cmd = strtok(line, ";");
        if (next_cmd != NULL) {
            char *ptr = strtok(NULL, ";");
            while (ptr != NULL) {
                strcat(buf, ptr);
                strcat(buf, ";");
                ptr = strtok(NULL, ";");
            }
        }
        char *word = strtok(next_cmd, " ");
        if (word == NULL) word = next_cmd;
        char **words = NULL;
        int words_len = 0;
        while (word != NULL) {
            words = realloc(words, sizeof(words) + sizeof(char *));
            words[words_len] = word;
            words_len++;
            word = strtok(NULL, " ;");
        }
        return_code = cmd_executor(words, words_len);
        free(line);
    }
}

int main(int argc, char *argv[]) {
    cmd_parser();
    return 0;
}