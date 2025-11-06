#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "command.h"
#include "autoComplete.h"
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


int match_prefix(const char *prefix, const char *cmd) {
    return strncmp(cmd, prefix, strlen(prefix)) == 0;
}

const char *get_autocomplete_suggestion(const char *input) {
    for (int i = 0; builtin_commands[i] != NULL; i++) {
        if (match_prefix(input, builtin_commands[i])) {
            return builtin_commands[i];
        }
    }
    return NULL;
}

#define COLOR_DIM "\033[90m"
#define COLOR_RESET "\033[0m"

const char *get_best_match(const char *input) {
    // If input has spaces, we assume last token is a path
    const char *last_space = strrchr(input, ' ');
    if (last_space) {
        const char *path_part = last_space + 1;
        const char *path_suggestion = get_best_path_match(path_part);
        return path_suggestion ? path_suggestion : NULL;
    }

    // Otherwise, do normal command suggestions
    const char *best = NULL;
    int best_len = 0;

    for (int i = 0; builtin_commands[i] != NULL; i++) {
        if (strncmp(builtin_commands[i], input, strlen(input)) == 0) {
            if (!best || strlen(builtin_commands[i]) < best_len) {
                best = builtin_commands[i];
                best_len = strlen(best);
            }
        }
    }

    for (int i = 0; i < autocomplete_path_count(); i++) {
        const char *cmd = autocomplete_path_get(i);
        if (strncmp(cmd, input, strlen(input)) == 0) {
            if (!best || strlen(cmd) < best_len) {
                best = cmd;
                best_len = strlen(best);
            }
        }
    }

    return best;
}


void print_all_matches(const char *input) {
    printf("\n");
    // Built-ins
    for (int i = 0; builtin_commands[i] != NULL; i++) {
        if (strncmp(builtin_commands[i], input, strlen(input)) == 0)
            printf("%s  ", builtin_commands[i]);
    }
    // PATH commands
    for (int i = 0; i < autocomplete_path_count(); i++) {
        const char *cmd = autocomplete_path_get(i);
        if (strncmp(cmd, input, strlen(input)) == 0)
            printf("%s  ", cmd);
    }
    printf("\n");
}




void repaint_line(const char *command) {
    printf("\r\033[K");       
    show_prompt();            
    printf("%s", command);    
    const char *match = get_best_match(command);
    if (match && strcmp(match, command) != 0) {
        const char *last_space = strrchr(command, ' ');
        if (last_space) {
            const char *last_token = last_space + 1;
            char expanded[PATH_MAX];
            if (last_token[0] == '~') {
                const char *home = getenv("HOME");
                if (!home) home = "/";
                if (last_token[1] == '/' || last_token[1] == '\0')
                    snprintf(expanded, sizeof(expanded), "%s%s", home, last_token + 1);
                else
                    snprintf(expanded, sizeof(expanded), "%s/%s", home, last_token + 1);
            } else {
                snprintf(expanded, sizeof(expanded), "%s", last_token);
            }

            size_t elen = strlen(expanded);
            if (strncmp(match, expanded, elen) == 0) {
                printf(COLOR_DIM "%s" COLOR_RESET, match + elen);
            } else {
                const char *m_basename = strrchr(match, '/');
                m_basename = m_basename ? m_basename + 1 : match;
                size_t tlen = strlen(last_token);
                if (strncmp(m_basename, last_token, tlen) == 0) {
                    const char *pos = strstr(match, m_basename);
                    if (pos) printf(COLOR_DIM "%s" COLOR_RESET, pos + tlen);
                } else {
                    printf(COLOR_DIM "%s" COLOR_RESET, match);
                }
            }
        } else {
            printf(COLOR_DIM "%s" COLOR_RESET, match + strlen(command));
        }
    }
    fflush(stdout);
}


// bool is_path_context(const char *input) {
//     char first_word[128];
//     int i = 0;

//     // extract the first word (command)
//     while (input[i] && !isspace(input[i]) && i < (int)(sizeof(first_word) - 1)) {
//         first_word[i] = input[i];
//         i++;
//     }
//     first_word[i] = '\0';

//     for (int j = 0; path_commands[j]; j++) {
//         if (strcmp(first_word, path_commands[j]) == 0)
//             return true;
//     }
//     return false;
// }


int main() {
    char command[1024];
    struct termios orig_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    system("clear");
    // initialize autocomplete (populate PATH commands)
    autocomplete_init();

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
                if (seq[1] == 'A') { // ↑
                    // (your history up code remains)
                } 
                else if (seq[1] == 'B') { // ↓
                    // (your history down code remains)
                } 
                else if (seq[1] == 'C') { // → key = accept suggestion
                    const char *match = get_best_match(command);
                    if (match && strcmp(match, command) != 0) {
                        // replace only the last token in command with match
                        char newcmd[1024];
                        const char *last_space = strrchr(command, ' ');
                        if (last_space) {
                            size_t prefix_len = (size_t)(last_space - command) + 1; // include space
                            if (prefix_len >= sizeof(newcmd)) prefix_len = sizeof(newcmd) - 1;
                            memcpy(newcmd, command, prefix_len);
                            newcmd[prefix_len] = '\0';
                            strncat(newcmd, match, sizeof(newcmd) - strlen(newcmd) - 1);
                        } else {
                            // no space, replace whole command
                            strncpy(newcmd, match, sizeof(newcmd));
                            newcmd[sizeof(newcmd)-1] = '\0';
                        }
                        // copy back into command buffer
                        strncpy(command, newcmd, 1024);
                        command[1023] = '\0';
                        printf("\r\033[K");
                        show_prompt();
                        printf("%s", command);
                        fflush(stdout);
                        pos = strlen(command);
                    }
                }
            }
        } 
        else if (c == 9) {  // TAB pressed
            const char *last_space = strrchr(command, ' ');
            if (last_space) {
                const char *path_part = last_space + 1;
                print_all_path_matches(path_part);
            } else {
                print_all_matches(command);
            }
            repaint_line(command);
        }
        else if (c == 127) { // BACKSPACE
        if (pos > 0) {
            pos--;
            command[pos] = '\0';
            repaint_line(command);
        }
    }
        else {
            if (pos < (int)(sizeof(command) - 1)) {
                command[pos++] = c;
                command[pos] = '\0';
                printf("\r\033[K");
                show_prompt();
                printf("%s", command);
    
                // show inline dim suggestion
                const char *match = get_best_match(command);
                if (match && strcmp(match, command) != 0) {
                    printf(COLOR_DIM "%s" COLOR_RESET, match + strlen(command));
                }
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

    autocomplete_shutdown();
    free(history);
    return 0;
}
