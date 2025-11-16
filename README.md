# Shell — Custom Unix-like Shell

A small educational Unix-like shell implemented in C. It accepts user input, parses commands and arguments, executes built-in commands (cd, ls, pwd, touch, rm, rmdir, etc.) and external programs via fork/exec. It also provides piping, redirection, command history, basic job control, and signal handling.

## Features

- Prompt with compacted current directory: `user@host <path> >`
- Built-in commands: `cd`, `ls`, `pwd`, `touch`, `rm`, `rmdir`, `help`, `source`, `nano`, `clear`, `exit`, and more.
- Execute external programs using `fork()` + `execvp()` (supports absolute and relative paths).
- Piping (`|`) and redirection (`>`, `>>`, `<`) — handled by existing helpers.
- In-memory command history with Up/Down arrow navigation.
- Basic job control:
  - Run commands in background using `&`.
  - `jobs` builtin to list background/stopped jobs.
  - `fg` and `bg` builtins to bring jobs to foreground or resume them in background.
- Signal handling:
  - Ctrl+C forwards SIGINT to the foreground job.
  - Ctrl+Z forwards SIGTSTP to the foreground job and marks it stopped.
- Auto-completion suggestions and inline hints (basic).

## Status / Notes

This project implements the core shell functionality and many advanced features. A few important caveats remain:

- Quoting and escaping: Current tokenization is simple whitespace-based. Quoted strings (single/double quotes) and escaping are not fully supported and should be improved for production-like behavior.
- Full job-control semantics: Basic background jobs, `jobs`, `fg`, `bg` are implemented. For full POSIX job control support (process groups, terminal ownership via `tcsetpgrp`, and robust `SIGCHLD` reaping), enhancements are recommended.
- Multi-file build: Some files in the repository currently contain overlapping function definitions. To build everything together cleanly you may need to consolidate duplicate implementations (pick one `main`/implementation or split into proper headers/translation units).

## Build

From the project root (`/home/zohaib/Uni/OS/Shell_Project`) you can compile the current `main.c` with:

```sh
gcc -D_GNU_SOURCE -std=c11 -Wall -Wextra -o myshell /home/zohaib/Uni/OS/Shell_Project/main.c -I/home/zohaib/Uni/OS/Shell_Project -lm
```

Notes:
- The project uses GNU extensions (e.g. `asprintf`) so `-D_GNU_SOURCE` is helpful.
- If you want to build all `.c` files in the repo, first ensure duplicate symbol issues are resolved (see "Status / Notes").

## Run

Start the shell:

```sh
./myshell
```

Try some commands:

```sh
# Run in foreground
ls -l /tmp

# Run in background
sleep 30 &
jobs

# Bring job to foreground (jid shown by jobs)
fg %1

# Stop foreground then resume in background
# (Start a long-running process, then press Ctrl+Z)
bg %1

# Redirection
echo "hello world" > out.txt
cat < out.txt

# Pipe
ls -la | grep ".c"
```

## Examples

- Use Up/Down arrows to navigate history.
- Press Tab to show suggestions (basic autocomplete).

## Files changed by recent work

- `main.c` — updated to detect trailing `&` for background jobs; added `jobs`, `fg`, `bg` builtins; set `fg_pid` to forward signals to foreground jobs.
- `command.h` — added a small in-memory job table and helper functions (add/remove/find/list/mark), and ensured necessary feature macros/includes.

## Recommended next improvements

1. Implement a tokenizer that correctly handles quoted strings and escape sequences.
2. Add a `SIGCHLD` handler to reap children and notify about job completion (avoid zombies and stale job entries).
3. Use `setpgid()` and `tcsetpgrp()` to implement full job control and better terminal handling for interactive programs.
4. Consolidate source files to avoid duplicate definitions so the project builds cleanly as a multi-file program.
5. Persist history to `~/.myshell_history` and add a `history` builtin.

## Contributing / Testing

- If you add features, please add a small manual test script demonstrating the change (e.g., tests for piping, redirection, job control).
- For immediate checks, compile `main.c` as shown and run `./myshell`.

## License

This is educational code. Add a license file (e.g., `MIT`) if you plan to publish or share it publicly.

---

If you want, I can now:
- Implement proper quoting/escaping parsing, or
- Add a `SIGCHLD` handler to reap and report job status, or
- Clean up duplicate definitions so the full repo builds with a single `gcc` command.

Tell me which you'd like next and I'll implement it.