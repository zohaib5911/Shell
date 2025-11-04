#include <pwd.h>
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bits/local_lim.h>

void show_prompt() {
    char *user = getenv("USER");
    if (!user) user = getpwuid(getuid())->pw_name;

    char hostname[HOST_NAME_MAX + 1];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "localhost");
    }

    char *cwd = getcwd(NULL, 0);
    if (!cwd) {
        perror("getcwd");
        return;
    }

    char *home = getenv("HOME");
    char display_path[PATH_MAX];
    if (home && strncmp(cwd, home, strlen(home)) == 0) {
        snprintf(display_path, sizeof(display_path), "~%s", cwd + strlen(home));
    } else {
        snprintf(display_path, sizeof(display_path), "%s", cwd);
    }
    char compact_path[PATH_MAX] = "";
    if (display_path[0] != '~') {
        char temp[PATH_MAX];
        strncpy(temp, display_path, sizeof(temp));
        temp[sizeof(temp) - 1] = '\0';

        char *token = strtok(temp, "/");
        while (token) {
            if (strlen(compact_path) > 0)
                strcat(compact_path, "/");

            if (strlen(token) > 1)
                strncat(compact_path, token, 1);
            else
                strcat(compact_path, token);

            token = strtok(NULL, "/");
        }
    } else {
        strncpy(compact_path, display_path, sizeof(compact_path));
        compact_path[sizeof(compact_path) - 1] = '\0';
    }

    printf("%s@%s %s > ", user, hostname, compact_path);
    free(cwd);
}
