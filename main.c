#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/time.h>
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
    struct timeval last_tab_time = {0,0};
    int last_tab = 0;
    tcgetattr(STDIN_FILENO, &orig_termios);
    system("clear");

    // Register implemented/known commands so completion can prioritize them.
    // Add more calls here for each command you implement.
    register_builtin("cd");
    register_builtin("ls");
    register_builtin("pwd");
    register_builtin("touch");
    register_builtin("rm");
    register_builtin("help");
    register_builtin("exit");
    register_builtin("clear");
    register_builtin("deactivate");
    register_builtin("source");
    register_builtin("nano");
    register_builtin("rmdir");
    // example commands you listed — add your implemented commands here
    register_builtin("clrunimap");
    register_builtin("clockdiff");
    register_builtin("cltest");
    register_builtin("cllayerinfo");
    register_builtin("classes");

    while (1) {
        show_prompt();
        fflush(stdout);

        enable_raw_mode(&orig_termios);

        int pos = 0;
        command[0] = '\0';
        int c;

        while ((c = getchar()) != '\n') {
            if (c == 27) { // ↑ ↓ keys
                char seq[3];
                seq[0] = getchar();
                seq[1] = getchar();
                seq[2] = '\0';

                if (seq[0] == '[') {
                    if (seq[1] == 'A') { // UP
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
                                        // show inline dim suggestion for this restored command
                                        {
                                            char matches[200][256];
                                            int mc = collect_completions(command, pos, matches, 200);
                                            if (mc > 0) {
                                                // compute current token length
                                                int token_start = pos - 1;
                                                while (token_start >= 0 && command[token_start] != ' ' && command[token_start] != '\t') token_start--;
                                                token_start++;
                                                int token_len = pos - token_start;
                                                if (token_len < 0) token_len = 0;
                                                // print remainder of first match in dim
                                                if ((int)strlen(matches[0]) > token_len) {
                                                    int suf = (int)strlen(matches[0]) - token_len;
                                                    printf("\033[2m%s\033[0m", matches[0] + token_len);
                                                    // move cursor back by suf
                                                    printf("\033[%dD", suf);
                                                    fflush(stdout);
                                                }
                                            }
                                        }
                            }
                        }
                    }
                    else if (seq[1] == 'B') { // DOWN
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
                                        // show inline dim suggestion
                                        {
                                            char matches[200][256];
                                            int mc = collect_completions(command, pos, matches, 200);
                                            if (mc > 0) {
                                                int token_start = pos - 1;
                                                while (token_start >= 0 && command[token_start] != ' ' && command[token_start] != '\t') token_start--;
                                                token_start++;
                                                int token_len = pos - token_start;
                                                if (token_len < 0) token_len = 0;
                                                if ((int)strlen(matches[0]) > token_len) {
                                                    int suf = (int)strlen(matches[0]) - token_len;
                                                    printf("\033[2m%s\033[0m", matches[0] + token_len);
                                                    printf("\033[%dD", suf);
                                                    fflush(stdout);
                                                }
                                            }
                                        }
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
            else if (c == '\t') {  // 🟢 TAB pressed — autocomplete (single-tab: common prefix; double-tab: list)
                // collect matches for token under cursor
                char matches[200][256];
                int max_matches = 200;
                int match_count = collect_completions(command, pos, matches, max_matches);

                if (match_count == 0) {
                    // no matches: do nothing
                } else if (match_count == 1) {
                    // single match: replace the current token with the single candidate (use matches[0])
                    int token_start = pos - 1;
                    while (token_start >= 0 && command[token_start] != ' ' && command[token_start] != '\t') token_start--;
                    token_start++;
                    int token_len = pos - token_start;
                    if (token_len < 0) token_len = 0;

                    // build new command: prefix + matches[0] + suffix
                    char newcmd[1024];
                    int prefix_len = token_start;
                    if (prefix_len > 1023) prefix_len = 1023;
                    memcpy(newcmd, command, prefix_len);
                    newcmd[prefix_len] = '\0';
                    // append match
                    strncat(newcmd, matches[0], sizeof(newcmd) - strlen(newcmd) - 1);
                    // append suffix (rest of original command after cursor)
                    if (pos < (int)strlen(command)) {
                        strncat(newcmd, command + pos, sizeof(newcmd) - strlen(newcmd) - 1);
                    }

                    // commit and redraw
                    printf("\r\033[K");
                    show_prompt();
                    snprintf(command, sizeof(command), "%s", newcmd);
                    printf("%s", command);
                    fflush(stdout);
                    pos = token_start + (int)strlen(matches[0]);

                    last_tab = 0;
                } else {
                    // multiple matches: compute longest common prefix among matches
                    int common_len = strlen(matches[0]);
                    for (int i = 1; i < match_count; ++i) {
                        int j = 0;
                        while (j < common_len && matches[0][j] && matches[i][j] && matches[0][j] == matches[i][j]) j++;
                        common_len = j;
                    }
                    // determine current token
                    int token_start = pos - 1;
                    while (token_start >= 0 && command[token_start] != ' ' && command[token_start] != '\t') token_start--;
                    token_start++;
                    int token_len = pos - token_start;
                    if (token_len < 0) token_len = 0;

                    if (common_len > token_len) {
                        // append the extra common prefix characters from matches[0]
                        int add_len = common_len - token_len;
                        if (add_len > 0) {
                            // ensure we don't overflow command buffer
                            int can_add = (int)sizeof(command) - pos - 1;
                            if (add_len > can_add) add_len = can_add;
                            // copy characters from matches[0]
                            for (int i = 0; i < add_len; ++i) {
                                command[pos + i] = matches[0][token_len + i];
                            }
                            pos += add_len;
                            command[pos] = '\0';
                            // redraw line
                            printf("\r\033[K");
                            show_prompt();
                            printf("%s", command);
                            fflush(stdout);
                        }
                        last_tab = 0;
                    } else {
                        // common prefix didn't grow: if user pressed Tab twice quickly, show list in dim
                        struct timeval now;
                        gettimeofday(&now, NULL);
                        long diff_ms = (now.tv_sec - last_tab_time.tv_sec) * 1000 + (now.tv_usec - last_tab_time.tv_usec) / 1000;
                        if (last_tab && diff_ms < 800) {
                            // print matches in dim and reprint prompt + current command
                            printf("\n");
                            print_completions_dim(matches, match_count);
                            show_prompt();
                            printf("%s", command);
                            fflush(stdout);
                            last_tab = 0;
                        } else {
                            // mark this as first tab press and store time
                            gettimeofday(&last_tab_time, NULL);
                            last_tab = 1;
                        }
                    }
                }
            }
            else if (c == 127) { // Backspace
                if (pos > 0) {
                    pos--;
                    command[pos] = '\0';
                    // redraw line and show suggestion
                    printf("\r\033[K");
                    show_prompt();
                    printf("%s", command);
                    fflush(stdout);
                    // inline suggestion
                    {
                        char matches[200][256];
                        int mc = collect_completions(command, pos, matches, 200);
                        if (mc > 0) {
                            int token_start = pos - 1;
                            while (token_start >= 0 && command[token_start] != ' ' && command[token_start] != '\t') token_start--;
                            token_start++;
                            int token_len = pos - token_start;
                            if (token_len < 0) token_len = 0;
                            if ((int)strlen(matches[0]) > token_len) {
                                int suf = (int)strlen(matches[0]) - token_len;
                                printf("\033[2m%s\033[0m", matches[0] + token_len);
                                printf("\033[%dD", suf);
                                fflush(stdout);
                            }
                        }
                    }
                }
            }
            else if (c == 4) { // Ctrl+D (Exit)
                disable_raw_mode(&orig_termios);
                printf("\nExiting shell...\n");
                free(history);
                return 0;
            }
            else { // Normal input
                if (pos < (int)(sizeof(command) - 1)) {
                    command[pos++] = c;
                    command[pos] = '\0';
                    putchar(c);
                    fflush(stdout);
                    // inline suggestion: show dim remainder of first match if exists
                    {
                        char matches[200][256];
                        int mc = collect_completions(command, pos, matches, 200);
                        if (mc > 0) {
                            int token_start = pos - 1;
                            while (token_start >= 0 && command[token_start] != ' ' && command[token_start] != '\t') token_start--;
                            token_start++;
                            int token_len = pos - token_start;
                            if (token_len < 0) token_len = 0;
                            if ((int)strlen(matches[0]) > token_len) {
                                int suf = (int)strlen(matches[0]) - token_len;
                                printf("\033[2m%s\033[0m", matches[0] + token_len);
                                printf("\033[%dD", suf);
                                fflush(stdout);
                            }
                        }
                    }
                }
                current_history_index = -1;
            }
        }

    // Clear any inline dim suggestion before leaving raw mode / printing newline.
    // Redraw the prompt with the real command (no ghost text), then print newline.
    printf("\r\033[K");
    show_prompt();
    printf("%s", command);
    fflush(stdout);

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

        if (strcmp(command, "deactivate") == 0) {
            deactivate_virtualenv();
            continue;
        }

        commands_operator(command);
    }

    free(history);
    return 0;
}
