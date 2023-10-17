#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include "shell.h"

static int
argv_alloc(cmd_t *cmd, int cap)
{
        if (cap < 0) {
                fprintf(stderr, "Capacity must be larger or equal to 0.\n");
                return 1;
        }

        char *tmp = (char *) realloc((char *) cmd->argv, (size_t) cap * sizeof(char *));

        if (tmp == nullptr) {
                fprintf(stderr, "Couldn't allocate memory for tokens.\n");
                return 1;
        }

        cmd->argv = (char **) tmp;
        cmd->cap = cap;

        return 0;
}

static int
cmd_alloc(cmd_arr_t *cmd_arr, int cap)
{
        if (cap < 0) {
                fprintf(stderr, "Capacity must be larger or equal to 0.\n");
                return 1;
        }

        cmd_t *tmp = (cmd_t *) realloc(cmd_arr->ptr, (size_t) cap * sizeof(cmd_t));

        if (tmp == nullptr) {
                fprintf(stderr, "Couldn't allocate memory for tokens.\n");
                return 1;
        }

        for (int i = cmd_arr->size; i < cap; i++) {
                tmp[i].argv = nullptr;
                tmp[i].argc = 0;
        }

        cmd_arr->ptr = tmp;
        cmd_arr->cap = cap;

        return 0;
}

static int
get_word(char **word, char *start, size_t n)
{
        char *tmp = (char *) calloc(n + 1, sizeof(char));

        if (tmp == nullptr) {
                fprintf(stderr, "Error: Couldn't allocate memory for word.\n");
                return 1;
        }

        memcpy(tmp, start, n);

        *word = tmp;

        return 0;
}

static char*
get_cmd(cmd_t *cmd, char *cmd_line)
{
        char *end_cmd = strpbrk(cmd_line, "|\n");

        //Empty line.
        if (end_cmd == nullptr) {
                return cmd_line;
        }

        get_word(&cmd->line, cmd_line, end_cmd - cmd_line);

        argv_alloc(cmd, 10);

        // strtok usage here.
        int argc = 0;
        char *new_ptr = strtok(cmd->line, BREAKSET);
        while (new_ptr) {
                cmd->argv[argc++] = new_ptr;

                if (cmd->cap < argc + 1) {
                        argv_alloc(cmd, cmd->cap * 2);
                }

                new_ptr = strtok(NULL, BREAKSET);
        }

        cmd->argv[argc] = nullptr;
        cmd->argc = argc;

        cmd_line = end_cmd + 1;
        if (*cmd_line == '\0') {
                return nullptr;
        }

        return cmd_line;
}

int
parser(char *cmd_line, size_t size, cmd_arr_t *cmd_arr)
{
        int cmd_count = 0;
        char *new_cmd_line;
        cmd_alloc(cmd_arr, 10);

        while (true) {
                while (isspace(*cmd_line++)) ;
                cmd_line--;

                // Empty line.
                if (*cmd_line == 0) {
                        break;
                }

                new_cmd_line = get_cmd(&cmd_arr->ptr[cmd_count], cmd_line);

                if (new_cmd_line == nullptr) {
                        cmd_count++;
                        break;
                }

                cmd_count++;
                cmd_line = new_cmd_line;

                if (cmd_arr->cap < cmd_count + 1)
                        cmd_alloc(cmd_arr, cmd_arr->cap * 2);
        }

        cmd_arr->size = cmd_count;

        return 0;
}

static void
pipe_dtor(int (*fds)[2], int n_pipes)
{
        for (int i = 0; i < n_pipes - 1; i++) {
                if (fds[i][1] != -1)
                        close(fds[i][1]);

                if (fds[i + 1][0] != -1)
                        close(fds[i + 1][0]);
        }

        free(fds);
}

static int
pipe_ctor(int (**fds_ptr)[2], int n_pipes)
{
        int (*fds)[2] = (int (*)[2]) malloc(2 * n_pipes * sizeof(int));
        if (fds == nullptr) {
                perror("pipe_ctor() ");
                exit(1);
        }

        fds[0][0] = STDIN_FILENO;
        fds[n_pipes - 1][1] = STDOUT_FILENO;

        for (int i = 0; i < n_pipes - 1; i++) {
                int next_pipe[2] = {};

                if (pipe(next_pipe) == -1) {
                        pipe_dtor(fds, n_pipes);
                        exit(1);
                }

                fds[i][1] = next_pipe[1];
                fds[i + 1][0] = next_pipe[0];
        }

        *fds_ptr = fds;

        return 0;
}

int
run(cmd_arr_t *cmd_arr)
{
        int (*fds)[2];
        pipe_ctor(&fds, cmd_arr->size);

        for (int i = 0; i < cmd_arr->size; i++) {
                cmd_t cmd = cmd_arr->ptr[i];

                if (fork() == 0) {
                        dup2(fds[i][0], STDIN_FILENO);
                        dup2(fds[i][1], STDOUT_FILENO);

                        pipe_dtor(fds, cmd_arr->size);

                        execvp(cmd.argv[0], cmd.argv);

                        return 0;
                }
        }

        pipe_dtor(fds, cmd_arr->size);
        for (int i = 0; i < cmd_arr->size; i++)
                wait(NULL);

        return 0;
}

[[maybe_unused]] void
cleanup(char *cmd_line, cmd_arr_t *cmd_arr)
{
        for (int i = 0; i < cmd_arr->size; i++) {
                free(cmd_arr->ptr[i].argv);
                free(cmd_arr->ptr[i].line);
        }
        free(cmd_arr->ptr);
        free(cmd_line);
}

