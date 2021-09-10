
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int returned;

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
int function_call(int count_words, char **words){
    if (!strcmp(words[0], "echo")) {
        return echo(count_words, words);
    } else
    if(!strcmp(words[0], "retcode")){
        return retcode(count_words,words)  ;
    }
    return returned;
}

char *get_string(int *len) {

    *len = 0;
    int capacity = 1;
    char *s = (char*) malloc(sizeof(char));
    char c = (char)getchar();
    if ( c != '\n' ) {
    }
    while (c != '\n' && c != ';' && c!=EOF) {
        s[(*len)++] = c;
        if (*len >= capacity) {
            capacity *= 2;
            s = (char*) realloc(s, capacity * sizeof(char));
        }
        c = (char)getchar();
    }
    s[*len] = '\0';
    return s;
}
int parsing (){
    while (1){
        int len;
        char *str = get_string(&len);
        int count_words = 0;
        char *buf = strtok(str, " ");
        char **words = NULL;
        while (buf!=NULL){
            words = realloc(words,(count_words+1)*sizeof (char*));
            words[count_words] = buf;
            count_words++;
            buf = strtok(NULL, " ");
        }
        if (words!=NULL) {
            returned = function_call(count_words, words);
            free(words);
        }
        free(str);
        if (feof(stdin))
            return 0;
    }
}

int main(int argc, char *argv[]) {
    int check = 0;
    parsing();
    return 0;
}
