#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
typedef struct {
    char *command;  
    int index;      
} HistoryEntry;

extern char *history; 

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

// Activate the virtual environment
void activate_virtualenv(const char *env_name) {
    
}

// tab key
// history mechanism
// tries for autocompletion
// tab key
// history mechanism
// tries fo


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

