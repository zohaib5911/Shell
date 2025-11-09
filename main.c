#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "command.h"
#include "promt.h"

char *history = NULL; 
int current_history_index = -1;


void commands_operator(char *input) {
    char *args[100];
    int i = 0;

    char *token = strtok(input, " \t\n");
    while (token && i < 99) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;

    if (args[0] == NULL) return;
    if (strcmp(args[0], "rmdir") == 0) {
        if (args[1] != NULL)
            rm_r_recursive(args[1]);
        else
            fprintf(stderr, "rmdir: missing directory operand\n");
        return;
    }
    if (strcmp(args[0], "cd") == 0) {
        if (args[1] != NULL)
            cd_commands(args[1]);
        else
            cd_commands("");
        return;
    }
    if (strcmp(args[0], "ls") == 0) {
        ls_commands(args);
        return;
    }

    if (strcmp(args[0], "pwd") == 0) {
        pwd_commands();
        return;
    }
    if( strcmp(args[0], "touch") == 0) {
        if (args[1] != NULL)
            touch_commands(args[1]);
        else
            fprintf(stderr, "touch: missing file operand\n");
        return;
    }
    if(strcmp(args[0], "rm") == 0) {
        if (args[1] != NULL)
            rm_commands(args[1]);
        else
            fprintf(stderr, "rm: missing file operand\n");
        return;
    }
    if (strcmp(args[0], "help") == 0) {
        help_commands();
        return;
    }   
    if(strcmp(args[1], "--version") == 0) {
        version_command(args[0]);
        return;
    }
    if(strcmp(args[0] , "source") == 0) {
        if(args[1] != NULL) {
            activate_virtualenv(args[1]);
        } else {
            fprintf(stderr, "source: missing virtual environment name\n");
        }
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        execvp(args[0], args);
        perror("myshell");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    } else {
        perror("fork");
    }
}


void enable_raw_mode(struct termios *orig_termios) {
    struct termios raw = *orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);     
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disable_raw_mode(struct termios *orig_termios) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig_termios);
}

int main() {
    char command[1024];
    struct termios orig_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    system("clear");
    while (1) {
        show_prompt();
        fflush(stdout);

        enable_raw_mode(&orig_termios);

        int pos = 0;
        command[0] = '\0';
        int c;

        while ((c = getchar()) != '\n') {
            if (c == 27) { 
                char seq[3];
                seq[0] = getchar(); 
                seq[1] = getchar();
                seq[2] = '\0';

                if (seq[0] == '[') {
                    if (seq[1] == 'A') { 
                        int total = count_commands();
                        if (total > 0) {
                            if (current_history_index == -1)
                                current_history_index = total - 1;
                            else if (current_history_index > 0)
                                current_history_index--;

                            HistoryEntry last = last_command(current_history_index);
                            if (last.command) {
                                printf("\r\033[K"); 
                                show_prompt();
                                strcpy(command, last.command);
                                printf("%s", command);
                                fflush(stdout);
                                pos = strlen(command);
                                free(last.command);
                            }
                        }
                    } 
                    else if (seq[1] == 'B') {  
                        int total = count_commands();
                        if (total > 0 && current_history_index != -1) {
                            if (current_history_index < total - 1)
                                current_history_index++;
                            else
                                current_history_index = total - 1;

                            HistoryEntry last = last_command(current_history_index);
                            if (last.command) {
                                printf("\r\033[K");
                                show_prompt();
                                strcpy(command, last.command);
                                printf("%s", command);
                                fflush(stdout);
                                pos = strlen(command);
                                free(last.command);
                            }
                        } else {
                            printf("\r\033[K");
                            show_prompt();
                            fflush(stdout);
                            pos = 0;
                            command[0] = '\0';
                            current_history_index = -1;
                        }
                    }
                }
            } 
            else if (c == 127) { 
                if (pos > 0) {
                    pos--;
                    command[pos] = '\0';
                    printf("\b \b");
                    fflush(stdout);
                }
            } 
            else {
                if (pos < (int)(sizeof(command) - 1)) {
                    command[pos++] = c;
                    command[pos] = '\0';
                    putchar(c);
                    fflush(stdout);
                }
                current_history_index = -1; 
            }
        }

        disable_raw_mode(&orig_termios);
        printf("\n");

        if (strcmp(command, "") == 0)
            continue;

        add_to_history(command);

        if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0)
            break;

        if (strcmp(command, "clear") == 0) {
            system("clear");
            continue;
        }

        commands_operator(command);
    }

    free(history);
    return 0;
}