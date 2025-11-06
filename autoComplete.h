#ifndef AUTOCOMPLETE_H
#define AUTOCOMPLETE_H
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>

// implemented commands
const char* builtin_commands[] = {
    "cd",
    "ls",
    "pwd",
    "touch",
    "rm",
    "rmdir",
    "version",
    "source",
    "nano",
    "help",
    "exit",
    "clear",
    "deactivate",
    "mkdir",
    NULL
};

static char **path_commands = NULL;
static int path_count = 0;
static int path_capacity = 0;

static void path_ensure_capacity(int mincap) {
    if (path_capacity >= mincap) return;
    int newcap = path_capacity == 0 ? 256 : path_capacity * 2;
    while (newcap < mincap) newcap *= 2;
    path_commands = (char**)realloc(path_commands, newcap * sizeof(char*));
    if (!path_commands) {
        perror("realloc");
        exit(1);
    }
    path_capacity = newcap;
}

static int path_has(const char *name) {
    for (int i = 0; i < path_count; ++i)
        if (strcmp(path_commands[i], name) == 0) return 1;
    return 0;
}

static void path_add(const char *name) {
    if (!name || name[0] == '\0') return;
    if (path_has(name)) return; // dedupe
    path_ensure_capacity(path_count + 1);
    path_commands[path_count++] = strdup(name);
}

/* Check if an entry is executable file (build fullpath and test access) */
static int is_executable_file(const char *dirpath, const char *entry_name) {
    if (!entry_name) return 0;
    char *full = NULL;
    if (asprintf(&full, "%s/%s", dirpath, entry_name) == -1) return 0;
    int ok = 0;
    struct stat st;
    if (stat(full, &st) == 0) {
        if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
            if (access(full, X_OK) == 0) ok = 1;
        }
    }
    free(full);
    return ok;
}

static void load_path_dir(const char *dirpath) {
    if (!dirpath || dirpath[0] == '\0') return;
    DIR *d = opendir(dirpath);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        // skip "." and ".."
        if (e->d_name[0] == '.') continue;
        if (is_executable_file(dirpath, e->d_name)) {
            path_add(e->d_name);
        }
    }
    closedir(d);
}

int autocomplete_init(void) {
    const char *path = getenv("PATH");
    if (!path) return -1;
    char *copy = strdup(path);
    if (!copy) return -1;
    char *saveptr = NULL;
    char *token = strtok_r(copy, ":", &saveptr);
    while (token) {
        load_path_dir(token);
        token = strtok_r(NULL, ":", &saveptr);
    }
    free(copy);
    return 0;
}

void autocomplete_shutdown(void) {
    if (path_commands) {
        for (int i = 0; i < path_count; ++i) free(path_commands[i]);
        free(path_commands);
        path_commands = NULL;
    }
    path_count = 0;
    path_capacity = 0;
}

void autocomplete_print_all(void) {
    printf("Built-ins:\n");
    for (int i = 0; builtin_commands[i]; ++i) printf("  %s\n", builtin_commands[i]);
    printf("\nPATH commands (count=%d):\n", path_count);
    for (int i = 0; i < path_count; ++i) {
        printf("  %s\n", path_commands[i]);
        if ((i+1) % 50 == 0) printf("\n"); // break for readability
    }
}

int autocomplete_path_count(void) { return path_count; }
const char* autocomplete_path_get(int idx) {
    if (idx < 0 || idx >= path_count) return NULL;
    return path_commands[idx];
}


static void extract_last_token(const char *input, char *out) {
    int len = strlen(input);
    int i = len - 1;

    // skip trailing spaces
    while (i >= 0 && isspace((unsigned char)input[i]))
        i--;

    // find start of the word
    int end = i;
    while (i >= 0 && !isspace((unsigned char)input[i]))
        i--;

    int start = i + 1;
    int word_len = end - start + 1;
    if (word_len <= 0) {
        out[0] = '\0';
        return;
    }

    strncpy(out, input + start, word_len);
    out[word_len] = '\0';
}

// Find all matches from built-ins and PATH commands that start with prefix
static char** find_matches(const char* prefix, int* count) {
    static char* matches[2048];  // buffer to hold matches
    *count = 0;
    int plen = strlen(prefix);

    // --- Built-ins ---
    for (int i = 0; builtin_commands[i]; i++) {
        if (strncmp(prefix, builtin_commands[i], plen) == 0)
            matches[(*count)++] = (char*)builtin_commands[i];
    }

    // --- PATH Commands ---
    for (int i = 0; i < path_count; i++) {
        if (strncmp(prefix, path_commands[i], plen) == 0)
            matches[(*count)++] = path_commands[i];
    }

    matches[*count] = NULL;
    return matches;
}

// Main autocomplete handler
static void handle_autocomplete(char *buffer) {
    char prefix[256];
    extract_last_token(buffer, prefix);

    if (strlen(prefix) == 0)
        return;

    int count = 0;
    char **matches = find_matches(prefix, &count);

    if (count == 0) {
        return; // no suggestions
    } else if (count == 1) {
        // Complete directly
        strcat(buffer, matches[0] + strlen(prefix));
        printf("\r> %s", buffer);
        fflush(stdout);
    } else {
        // Multiple matches — show suggestions
        printf("\n");
        for (int i = 0; i < count; i++) {
            printf("%s  ", matches[i]);
            if ((i + 1) % 6 == 0) printf("\n");
        }
        printf("\n> %s", buffer);
        fflush(stdout);
    }
}



#include <libgen.h>  // for dirname, basename

// --- File and Folder Suggestions ---

const char *get_best_path_match(const char *path_prefix) {
    static char suggestion[PATH_MAX];
    suggestion[0] = '\0';

    if (!path_prefix || path_prefix[0] == '\0')
        return NULL;

    char path_copy[PATH_MAX];
    // safely copy input
    snprintf(path_copy, sizeof(path_copy), "%s", path_prefix);

    // expand leading ~ to HOME
    if (path_copy[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) home = "/";
        char expanded[PATH_MAX];
        if (path_copy[1] == '/' || path_copy[1] == '\0')
            snprintf(expanded, sizeof(expanded), "%s%s", home, path_copy + 1);
        else
            snprintf(expanded, sizeof(expanded), "%s/%s", home, path_copy + 1);
        snprintf(path_copy, sizeof(path_copy), "%s", expanded);
    }

    const char *base = strrchr(path_copy, '/');
    const char *pattern = base ? base + 1 : path_copy;

    char dirpath[PATH_MAX];
    if (base) {
        // if the only slash is at position 0 (i.e. "/foo"), keep dirpath as "/"
        if (base == path_copy) {
            snprintf(dirpath, sizeof(dirpath), "/");
        } else {
            // copy up to but not including the last '/'
            size_t len = (size_t)(base - path_copy);
            if (len >= sizeof(dirpath)) len = sizeof(dirpath) - 1;
            memcpy(dirpath, path_copy, len);
            dirpath[len] = '\0';
        }
    } else {
        snprintf(dirpath, sizeof(dirpath), ".");
    }

    DIR *d = opendir(dirpath[0] ? dirpath : ".");
    if (!d) return NULL;

    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (pattern[0] == '\0') {
            // if pattern is empty (path ended with '/'), accept all non-hidden entries
            if (e->d_name[0] == '.') continue;
        }
        if (strncmp(e->d_name, pattern, strlen(pattern)) == 0) {
            if (strcmp(dirpath, "/") == 0) {
                snprintf(suggestion, sizeof(suggestion), "/%s", e->d_name);
            } else if (strcmp(dirpath, ".") == 0) {
                snprintf(suggestion, sizeof(suggestion), "%s", e->d_name);
            } else {
                snprintf(suggestion, sizeof(suggestion), "%s/%s", dirpath, e->d_name);
            }
            closedir(d);
            return suggestion;
        }
    }

    closedir(d);
    return NULL;
}

void print_all_path_matches(const char *path_prefix) {
    if (!path_prefix) return;

    char path_copy[PATH_MAX];
    snprintf(path_copy, sizeof(path_copy), "%s", path_prefix);

    // expand ~
    if (path_copy[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) home = "/";
        char expanded[PATH_MAX];
        if (path_copy[1] == '/' || path_copy[1] == '\0')
            snprintf(expanded, sizeof(expanded), "%s%s", home, path_copy + 1);
        else
            snprintf(expanded, sizeof(expanded), "%s/%s", home, path_copy + 1);
        snprintf(path_copy, sizeof(path_copy), "%s", expanded);
    }

    const char *base = strrchr(path_copy, '/');
    const char *pattern = base ? base + 1 : path_copy;

    char dirpath[PATH_MAX];
    if (base) {
        if (base == path_copy) {
            snprintf(dirpath, sizeof(dirpath), "/");
        } else {
            size_t len = (size_t)(base - path_copy);
            if (len >= sizeof(dirpath)) len = sizeof(dirpath) - 1;
            memcpy(dirpath, path_copy, len);
            dirpath[len] = '\0';
        }
    } else {
        snprintf(dirpath, sizeof(dirpath), ".");
    }

    DIR *d = opendir(dirpath[0] ? dirpath : ".");
    if (!d) return;

    struct dirent *e;
    printf("\n");
    while ((e = readdir(d)) != NULL) {
        if (pattern[0] == '\0') {
            if (e->d_name[0] == '.') continue;
        }
        if (strncmp(e->d_name, pattern, strlen(pattern)) == 0)
            printf("%s  ", e->d_name);
    }
    printf("\n");
    closedir(d);
}








#endif // AUTOCOMPLETE_H
