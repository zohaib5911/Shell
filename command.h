#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <X11/Xosdefs.h>
#include <linux/limits.h>


typedef struct {
    char *command;  
    int index;      
} HistoryEntry;

extern char *history; 

// Builtins registry: user can register implemented commands so autocomplete
// will prioritize them. Use register_builtin("name") from main().
#define MAX_BUILTINS 1024
static char *builtin_commands[MAX_BUILTINS];
static int builtin_count = 0;

void register_builtin(const char *name) {
    if (!name) return;
    if (builtin_count >= MAX_BUILTINS) return;
    builtin_commands[builtin_count++] = strdup(name);
}

// Optional: load builtins from a newline-separated file
void register_builtins_from_file(const char *path) {
    if (!path) return;
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        // trim newline
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        if (*line) register_builtin(line);
    }
    fclose(f);
}

int count_commands() {
    if (!history || *history == '\0')
        return 0;

    int count = 0;
    for (char *p = history; *p; ++p)
        if (*p == '\n')
            ++count;
    return count;
}

HistoryEntry last_command(int index) {
    HistoryEntry entry = {NULL, -1};
    if (!history || *history == '\0')
        return entry;

    int total = count_commands();
    if (total == 0)
        return entry;
    if (index < 0)
        index = 0;
    else if (index >= total)
        index = total - 1;
    char *start = history;
    int current = 0;

    for (char *p = history; *p; ++p) {
        if (*p == '\n') {
            if (current == index)
                break;
            start = p + 1;
            current++;
        }
    }
    char *end = strchr(start, '\n');
    size_t len = end ? (size_t)(end - start) : strlen(start);

    entry.command = malloc(len + 1);
    if (!entry.command) {
        perror("malloc failed");
        entry.index = -1;
        return entry;
    }

    strncpy(entry.command, start, len);
    entry.command[len] = '\0';
    entry.index = index;

    return entry;
}

void add_to_history(const char *command) {
    size_t old_len = history ? strlen(history) : 0;
    size_t cmd_len = strlen(command);
    size_t new_len = old_len + cmd_len + 2; 
    history = realloc(history, new_len);
    if (!history) {
        perror("realloc failed");
        exit(1);
    }
    if (old_len == 0)
        history[0] = '\0';
    strcat(history, command);
    strcat(history, "\n");
}

void cd_commands(char *path) {
    if (path == NULL || strcmp(path, "") == 0) {
        char *home = getenv("HOME");
        if (home == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return;
        }
        if (chdir(home) != 0) {
            perror("cd");
            return;
        }
    } else {
        if (chdir(path) != 0) {
            perror("cd");
            return;
        }
    }

    char *cwd = getcwd(NULL, 0);
    if (cwd != NULL) {
        free(cwd);
    } else {
        perror("getcwd");
    }
}

void ls_commands(char **args) {   
    pid_t pid = fork();
    if (pid == 0) {
        execvp("ls", args);  
        perror("myshell");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    } else {
        perror("fork");
    }
}

void pwd_commands() {
    char *cwd = getcwd(NULL, 0);
    if (cwd != NULL) {
        printf("%s\n", cwd);
        free(cwd);
    } else {
        perror("getcwd");
    }
}
void touch_commands(char *filename) {
    FILE *file = fopen(filename, "a");
    if (file == NULL) {
        perror("touch");
        return;
    }
    fclose(file);
}

void nano_commands(char *filename) {
    pid_t pid = fork();
    if (pid == 0) {
        execlp("nano", "nano", filename, NULL);
        perror("myshell");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    } else {
        perror("fork");
    }
}
void rm_commands(char *filename) {
    if (remove(filename) != 0) {
        perror("rm");
    }
}

void help_commands() {
    printf("Supported commands:\n");
    printf("  cd [dir]      - Change directory to 'dir' or home if no dir is given\n");
    printf("  ls            - List files in the current directory\n");
    printf("  pwd           - Print the current working directory\n");
    printf("  touch [file]  - Create an empty file named 'file'\n");
    printf("  help          - Show this help message\n");
    printf("  exit          - Exit the shell\n");
}


void rm_r_recursive(const char *path) {
    struct dirent *entry;
    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                rm_r_recursive(fullpath);  
            } else {
                remove(fullpath);
            }
        }
    }

    closedir(dir);
    if (rmdir(path) != 0) {
        perror("rmdir");
    }
}
// gcc,gdb,python --version
void version_command(const char* arg){
    pid_t pid = fork();
    if (pid == 0) {
        execlp(arg, arg, "--version", NULL);
        perror("myshell");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    } else {
        perror("fork");
    }
}



char* env_path = NULL;

void activate_virtualenv(const char *path) {
    char resolved[PATH_MAX];
    if (realpath(path, resolved) == NULL) {
        fprintf(stderr, "source: could not find '%s'\n", path);
        return;
    }

    // Set environment vars
    setenv("VIRTUAL_ENV", resolved, 1);

    char new_path[4096];
    snprintf(new_path, sizeof(new_path), "%s/bin:%s", resolved, getenv("PATH"));
    setenv("PATH", new_path, 1);

    // Store venv path safely
    if (env_path) free(env_path);
    env_path = strdup(resolved);

    printf("Activated virtual environment: %s\n", resolved);
}

void deactivate_virtualenv() {
    if (getenv("VIRTUAL_ENV")) {
        unsetenv("VIRTUAL_ENV");
    }

    // Don’t free env_path if the prompt or others might still use it
    // Instead, just mark it inactive
    if (env_path) {
        free(env_path);
        env_path = NULL;
    }

    printf("Deactivated virtual environment\n");
}

// Fish-like autocomplete: complete the token at cursor_pos in `input`.
// result will contain the new full line (input with the token replaced by the completion).
void autocomplete_command(const char *input, int cursor_pos, char *result, size_t result_size) {
    result[0] = '\0';
    if (!input || cursor_pos < 0) return;

    int len = (int)strlen(input);
    if (cursor_pos > len) cursor_pos = len;

    // Find token start: last space before cursor (or 0)
    int token_start = cursor_pos - 1;
    while (token_start >= 0 && input[token_start] != ' ' && input[token_start] != '\t')
        token_start--;
    token_start++;

    int token_len = cursor_pos - token_start;
    if (token_len < 0) token_len = 0;

    // extract token
    char token[1024];
    if (token_len >= (int)sizeof(token)) token_len = sizeof(token) - 1;
    memcpy(token, input + token_start, token_len);
    token[token_len] = '\0';

    // Prefix (before token) and suffix (after cursor)
    char prefix_line[1024];
    char suffix_line[1024];
    int before_len = token_start;
    if (before_len >= (int)sizeof(prefix_line)) before_len = sizeof(prefix_line) - 1;
    memcpy(prefix_line, input, before_len);
    prefix_line[before_len] = '\0';

    int suffix_len = len - cursor_pos;
    if (suffix_len >= (int)sizeof(suffix_line)) suffix_len = sizeof(suffix_line) - 1;
    memcpy(suffix_line, input + cursor_pos, suffix_len);
    suffix_line[suffix_len] = '\0';

    // 1) Special-case: if the command starts with "cd " and we're completing the path token
    if (strncmp(prefix_line, "cd ", 3) == 0 && token_start >= 3) {
        const char *dir_prefix = input + 3;
        DIR *d = opendir(".");
        if (!d) return;
        struct dirent *entry;
        struct stat st;
        while ((entry = readdir(d)) != NULL) {
            if (stat(entry->d_name, &st) == 0 && S_ISDIR(st.st_mode)) {
                if (strncmp(entry->d_name, dir_prefix, strlen(dir_prefix)) == 0) {
                    snprintf(result, result_size, "%s%s%s", prefix_line, entry->d_name, suffix_line);
                    closedir(d);
                    return;
                }
            }
        }
        closedir(d);
    }

    // 2) Search history for a line whose token at start matches (only consider lines that start with token when token is at position 0)
    if (history && *token) {
        char *copy = strdup(history);
        if (copy) {
            char *line = strtok(copy, "\n");
            while (line) {
                // find token in the line at the same token_start position
                if ((int)strlen(line) >= token_start) {
                    if (strncmp(line + token_start, token, strlen(token)) == 0) {
                        // build completed line
                        snprintf(result, result_size, "%s", line);
                        free(copy);
                        return;
                    }
                }
                line = strtok(NULL, "\n");
            }
            free(copy);
        }
    }

    // 3) Search PATH for executables when token is at the beginning of the line (no command before it)
    if (token_start == 0 && token[0] != '\0') {
        char *path = getenv("PATH");
        if (!path) return;
        char *path_copy = strdup(path);
        if (!path_copy) return;
        char *p = strtok(path_copy, ":");
        struct dirent *entry;
        while (p) {
            DIR *d = opendir(p);
            if (d) {
                while ((entry = readdir(d)) != NULL) {
                    if (strncmp(entry->d_name, token, strlen(token)) == 0) {
                        snprintf(result, result_size, "%s%s%s", prefix_line, entry->d_name, suffix_line);
                        closedir(d);
                        free(path_copy);
                        return;
                    }
                }
                closedir(d);
            }
            p = strtok(NULL, ":");
        }
        free(path_copy);
    }

    // nothing found: leave result empty (caller can decide)
}

// Collect possible completions for the token at cursor_pos.
// Each match is written into matches[i] (max length 255). Returns number of matches found.
int collect_completions(const char *input, int cursor_pos, char matches[][256], int max_matches) {
    if (!input || cursor_pos < 0 || max_matches <= 0) return 0;
    int len = (int)strlen(input);
    if (cursor_pos > len) cursor_pos = len;

    int token_start = cursor_pos - 1;
    while (token_start >= 0 && input[token_start] != ' ' && input[token_start] != '\t')
        token_start--;
    token_start++;

    int token_len = cursor_pos - token_start;
    if (token_len < 0) token_len = 0;
    if (token_len > 250) token_len = 250;

    char token[256];
    memcpy(token, input + token_start, token_len);
    token[token_len] = '\0';

    int count = 0;

    // Helper to add a candidate (avoid overflow) - we directly add items below.

    // 1) If completing after "cd ", suggest directories
    if (token_start >= 3 && strncmp(input, "cd ", 3) == 0) {
        DIR *d = opendir(".");
        if (!d) return 0;
        struct dirent *entry;
        struct stat st;
        while ((entry = readdir(d)) != NULL) {
            if (stat(entry->d_name, &st) == 0 && S_ISDIR(st.st_mode)) {
                if (strncmp(entry->d_name, token, strlen(token)) == 0) {
                    if (count < max_matches) {
                        snprintf(matches[count], 256, "%s", entry->d_name);
                        count++;
                    }
                }
            }
        }
        closedir(d);
        return count;
    }

    // 2) If token_start == 0, search PATH executables and history
    if (token_start == 0) {
        // builtins first (registered commands)
        if (token[0] && builtin_count > 0) {
            for (int i = 0; i < builtin_count; ++i) {
                if (strncmp(builtin_commands[i], token, strlen(token)) == 0) {
                    if (count < max_matches) {
                        snprintf(matches[count], 256, "%s", builtin_commands[i]);
                        count++;
                    }
                }
            }
        }

        // history entries (full line)
        if (history && *token) {
            char *copy = strdup(history);
            if (copy) {
                char *line = strtok(copy, "\n");
                while (line) {
                    if (strncmp(line, token, strlen(token)) == 0) {
                        if (count < max_matches) {
                            snprintf(matches[count], 256, "%s", line);
                            count++;
                        }
                    }
                    line = strtok(NULL, "\n");
                }
                free(copy);
            }
        }

        char *path = getenv("PATH");
        if (path) {
            char *path_copy = strdup(path);
            if (path_copy) {
                char *p = strtok(path_copy, ":");
                struct dirent *entry;
                while (p) {
                    DIR *d = opendir(p);
                    if (d) {
                        while ((entry = readdir(d)) != NULL) {
                            if (strncmp(entry->d_name, token, strlen(token)) == 0) {
                                if (count < max_matches) {
                                    snprintf(matches[count], 256, "%s", entry->d_name);
                                    count++;
                                }
                            }
                        }
                        closedir(d);
                    }
                    p = strtok(NULL, ":");
                }
                free(path_copy);
            }
        }
        return count;
    }

    // 3) Otherwise, complete filenames/directories in current working directory or path-like tokens
    {
        // If token contains a slash, treat as path
        if (strchr(token, '/')) {
            // separate dir and partial
            char dirpart[PATH_MAX];
            char basepart[PATH_MAX];
            char *slash = strrchr(token, '/');
            if (slash == token) {
                // starts with /
                strcpy(dirpart, "/");
                strcpy(basepart, token + 1);
            } else {
                int dp_len = (int)(slash - token);
                if (dp_len >= (int)sizeof(dirpart)) dp_len = sizeof(dirpart)-1;
                memcpy(dirpart, token, dp_len);
                dirpart[dp_len] = '\0';
                strncpy(basepart, slash + 1, sizeof(basepart)-1);
                basepart[sizeof(basepart)-1] = '\0';
            }
            DIR *d = opendir(dirpart[0] ? dirpart : ".");
            if (!d) return count;
            struct dirent *entry;
            while ((entry = readdir(d)) != NULL) {
                if (strncmp(entry->d_name, basepart, strlen(basepart)) == 0) {
                    if (count < max_matches) {
                        if (dirpart[0])
                            snprintf(matches[count], 256, "%s/%s", dirpart, entry->d_name);
                        else
                            snprintf(matches[count], 256, "%s", entry->d_name);
                        count++;
                    }
                }
            }
            closedir(d);
            return count;
        }

        DIR *d = opendir(".");
        if (!d) return count;
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (strncmp(entry->d_name, token, strlen(token)) == 0) {
                if (count < max_matches) {
                    snprintf(matches[count], 256, "%s", entry->d_name);
                    count++;
                }
            }
        }
        closedir(d);
    }

    return count;
}

// Print matches in a dim style (like fish suggestions list)
void print_completions_dim(char matches[][256], int count) {
    if (count <= 0) return;
    // dim code: \033[2m ; reset: \033[0m
    for (int i = 0; i < count; ++i) {
        printf("\033[2m%s\033[0m\n", matches[i]);
    }
}





// tab key
// history mechanism
// tries for autocompletion  (fish)
// piping (bash)
// redirection (bash)
// Ctrl+C handling + Ctrl+Z handling
// left key for autocompletion
// --version command
// pip install <package>
// sudo command
// yay command
// pacman command
// git command
// making executable files and running them
// create and control environment 
// activate and deactivate virtual environments
// shutdown command + restart command
// open any app from the shell
// neofetch command + fastfetch command
// storage analysis command (like ncdu)
// process monitoring command (like htop)
// process killing command (like killall, pkill)
// network monitoring command (like iftop, nethogs)
// open website from shell
// cut , copy , paste commands by shell
// calendar command
// alarm command
// todo list command
// notes command
// weather command
// news command
// time command
// date command
// calculator command
// dictionary command
// translate command
// file search command

