#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "command.h"
#include "promt.h"

char *history = NULL; 
int current_history_index = -1;

#include <signal.h>
#include <sys/wait.h>

pid_t fg_pid = 0; // global foreground pid

volatile sig_atomic_t sigint_flag = 0;
volatile sig_atomic_t sigtstp_flag = 0;

void sigint_handler(int sig) {
    (void)sig;
    if (fg_pid != 0) {
        kill(fg_pid, SIGINT); 
    }
    const char nl = '\n';
    write(STDOUT_FILENO, &nl, 1);
    sigint_flag = 1;
}

void sigtstp_handler(int sig) {
    (void)sig;
    if (fg_pid != 0) {
        kill(fg_pid, SIGTSTP); 
    }
    const char msg[] = "\n[Stopped]\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    sigtstp_flag = 1;
}

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

    // handle background operator '&' (if last token)
    int background = 0;
    int argc = i;
    if (argc > 0 && strcmp(args[argc - 1], "&") == 0) {
        background = 1;
        args[argc - 1] = NULL;
        argc--;
    }

    // ------------------ PIPE HANDLING ------------------
    int pipe_index = -1;
    for (int j = 0; args[j] != NULL; j++) {
        if (strcmp(args[j], "|") == 0) {
            pipe_index = j;
            break;
        }
    }

    if (pipe_index != -1) {
        char *cmd1_args[pipe_index + 1];
        for (int i = 0; i < pipe_index; i++)
            cmd1_args[i] = args[i];
        cmd1_args[pipe_index] = NULL;

        int cmd2_len = 0;
        for (int i = pipe_index + 1; args[i] != NULL; i++)
            cmd2_len++;
        char *cmd2_args[cmd2_len + 1];
        for (int i = 0; i < cmd2_len; i++)
            cmd2_args[i] = args[pipe_index + 1 + i];
        cmd2_args[cmd2_len] = NULL;

        Pipe_commands(cmd1_args, cmd2_args);
        return;
    }

    // ------------------ REDIRECTION HANDLING (<, >, >>) ------------------
    char *output_file = NULL;
    char *input_file = NULL;
    int append = 0;

    for (int j = 0; args[j] != NULL; j++) {
        if (strcmp(args[j], ">") == 0) {
            output_file = args[j + 1];
            append = 0;
            args[j] = NULL;
            break;
        }
        else if (strcmp(args[j], ">>") == 0) {
            output_file = args[j + 1];
            append = 1;
            args[j] = NULL;
            break;
        }
        else if (strcmp(args[j], "<") == 0) {
            input_file = args[j + 1];
            args[j] = NULL;
            break;
        }
    }

    if (output_file || input_file) {
        redirect_commands(args, output_file, input_file, append);
        return;
    }

    // ------------------ BUILT-IN COMMANDS ------------------
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

    if (strcmp(args[0], "ls") == 0 && args[1] == NULL) {
        ls_commands(args);
        return;
    }

    if (strcmp(args[0], "pwd") == 0 && args[1] == NULL) {
        pwd_commands();
        return;
    }

    if (strcmp(args[0], "touch") == 0) {
        if (args[1] != NULL)
            touch_commands(args[1]);
        else
            fprintf(stderr, "touch: missing file operand\n");
        return;
    }

    if (strcmp(args[0], "rm") == 0) {
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

    if (args[1] && strcmp(args[1], "--version") == 0) {
        version_command(args[0]);
        return;
    }

    if (strcmp(args[0], "source") == 0) {
        if (args[1] != NULL)
            activate_virtualenv(args[1]);
        else
            fprintf(stderr, "source: missing virtual environment name\n");
        return;
    }

    // --- job control builtins ---
    if (strcmp(args[0], "jobs") == 0) {
        list_jobs();
        return;
    }

    if (strcmp(args[0], "fg") == 0) {
        // fg [%%jid | pid]
        Job *j = NULL;
        if (args[1] == NULL) {
            // pick most recent job (job_list head)
            if (job_list) j = job_list;
        } else {
            int jid = 0;
            if (args[1][0] == '%') jid = atoi(args[1] + 1);
            else jid = atoi(args[1]);
            if (jid > 0) j = find_job_by_jid(jid);
        }
        if (!j) {
            fprintf(stderr, "fg: no such job\n");
            return;
        }
        // continue and wait
        kill(j->pid, SIGCONT);
        mark_job_running(j->pid);
        fg_pid = j->pid;
        int status = 0;
        waitpid(j->pid, &status, WUNTRACED);
        if (WIFSTOPPED(status)) {
            mark_job_stopped(j->pid);
        } else {
            // finished
            remove_job(j);
        }
        fg_pid = 0;
        return;
    }

    if (strcmp(args[0], "bg") == 0) {
        // bg [%jid | pid]
        Job *j = NULL;
        if (args[1] == NULL) {
            if (job_list) j = job_list;
        } else {
            int jid = 0;
            if (args[1][0] == '%') jid = atoi(args[1] + 1);
            else jid = atoi(args[1]);
            if (jid > 0) j = find_job_by_jid(jid);
        }
        if (!j) {
            fprintf(stderr, "bg: no such job\n");
            return;
        }
        kill(j->pid, SIGCONT);
        mark_job_running(j->pid);
        printf("[%d] %d\n", j->jid, j->pid);
        return;
    }

    // ------------------ NORMAL EXECUTION ------------------
    pid_t pid = fork();
    if (pid == 0) {
        // child
        execvp(args[0], args);
        perror("myshell");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // parent
        if (background) {
            int jid = add_job(pid, input, JOB_RUNNING);
            if (jid < 0)
                fprintf(stderr, "failed to add background job\n");
            else
                printf("[%d] %d\n", jid, pid);
            // do not wait
        } else {
            fg_pid = pid;
            int status = 0;
            waitpid(pid, &status, WUNTRACED);
            if (WIFSTOPPED(status)) {
                // add to job list as stopped
                add_job(pid, input, JOB_STOPPED);
                printf("\n[%d] Stopped %d %s\n", next_jid-1, pid, input);
            } else {
                // if finished, ensure it's removed from jobs if present
                Job *j = find_job_by_pid(pid);
                if (j) remove_job(j);
            }
            fg_pid = 0;
        }
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
        signal(SIGINT, sigint_handler);
        signal(SIGTSTP, sigtstp_handler);
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