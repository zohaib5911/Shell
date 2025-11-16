#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <X11/Xosdefs.h>
#include <linux/limits.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>

typedef struct {
    char *command;  
    int index;      
} HistoryEntry;

// --- Simple job control implementation ---
typedef enum { JOB_RUNNING = 0, JOB_STOPPED = 1, JOB_DONE = 2 } JobStatus;

typedef struct Job {
    pid_t pid;
    int jid;
    char *cmdline;
    JobStatus status;
    struct Job *next;
} Job;

static Job *job_list = NULL;
static int next_jid = 1;

static Job *find_job_by_pid(pid_t pid) {
    for (Job *j = job_list; j; j = j->next) if (j->pid == pid) return j;
    return NULL;
}

static Job *find_job_by_jid(int jid) {
    for (Job *j = job_list; j; j = j->next) if (j->jid == jid) return j;
    return NULL;
}

static int add_job(pid_t pid, const char *cmdline, JobStatus status) {
    Job *j = malloc(sizeof(Job));
    if (!j) return -1;
    j->pid = pid;
    j->jid = next_jid++;
    j->cmdline = strdup(cmdline ? cmdline : "");
    j->status = status;
    j->next = job_list;
    job_list = j;
    return j->jid;
}

static void remove_job(Job *job) {
    if (!job) return;
    Job **p = &job_list;
    while (*p) {
        if (*p == job) {
            *p = job->next;
            free(job->cmdline);
            free(job);
            return;
        }
        p = &((*p)->next);
    }
}

static void list_jobs(void) {
    for (Job *j = job_list; j; j = j->next) {
        const char *st = j->status == JOB_RUNNING ? "Running" : (j->status == JOB_STOPPED ? "Stopped" : "Done");
        printf("[%d] %s %d %s\n", j->jid, st, j->pid, j->cmdline);
    }
}

static void mark_job_stopped(pid_t pid) {
    Job *j = find_job_by_pid(pid);
    if (j) j->status = JOB_STOPPED;
}

static void mark_job_running(pid_t pid) {
    Job *j = find_job_by_pid(pid);
    if (j) j->status = JOB_RUNNING;
}


extern char *history; 
char* env_path = NULL;


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
void activate_virtualenv(const char *path) {
    char resolved[PATH_MAX];
    if (realpath(path, resolved) == NULL) {
        fprintf(stderr, "source: could not find '%s'\n", path);
        return;
    }
    setenv("VIRTUAL_ENV", resolved, 1);
    char new_path[4096];
    snprintf(new_path, sizeof(new_path), "%s/bin:%s", resolved, getenv("PATH"));
    setenv("PATH", new_path, 1);
    if (env_path) free(env_path);
    env_path = strdup(resolved);
    printf("Activated virtual environment: %s\n", resolved);
}

void deactivate_virtualenv() {
    if (getenv("VIRTUAL_ENV")) {
        unsetenv("VIRTUAL_ENV");
    }
    if (env_path) {
        free(env_path);
        env_path = NULL;
    }
    printf("Deactivated virtual environment\n");
}


// ---------------- Pipe Execution -----------------

void Pipe_commands(char **cmd1_args, char **cmd2_args) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return;
    }

    pid_t pid1 = fork();
    if (pid1 == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        execvp(cmd1_args[0], cmd1_args);
        perror("execvp");
        exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[1]);
        close(pipefd[0]);
        execvp(cmd2_args[0], cmd2_args);
        perror("execvp");
        exit(1);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}



// Redirection >, <, >>
void redirect_commands(char **args, const char *output_file,
                       const char *input_file, int append)
{
    pid_t pid = fork();

    if (pid == 0) {
        // INPUT REDIRECTION (<)
        if (input_file) {
            int fd = open(input_file, O_RDONLY);
            if (fd < 0) {
                perror("open input");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        // OUTPUT REDIRECTION (> or >>)
        if (output_file) {
            int flags = O_CREAT | O_WRONLY | (append ? O_APPEND : O_TRUNC);
            int fd = open(output_file, flags, 0644);
            if (fd < 0) {
                perror("open output");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);

    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    } else {
        perror("fork");
    }
}



