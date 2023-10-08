#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include "shell.h"

static int
cmd_alloc(cmd_arr_t *cmd_arr, int cap)
{
        if (cap < 0) {
                fprintf(stderr, "Capacity must be larger or equal to 0.\n");
                return 1;
        }

        cmd_t *tmp = nullptr;
        tmp = (cmd_t *) realloc(cmd_arr->ptr, (size_t) cap * sizeof(cmd_t));

        if (tmp == nullptr) {
                fprintf(stderr, "Couldn't allocate memory for tokens.\n");
                return 1;
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
                fprintf(stderr, "Error: Couldn't allocate memory for tokens.\n");
                return 1;
        }

        memcpy(tmp, start, n);

        *word = tmp;

        return 0;
}

static char*
get_cmd(cmd_t *cmd, char *cmd_line)
{
        while (isspace(*cmd_line++)) ;

        cmd_line--;

        char *end_exec = strpbrk(cmd_line, BREAKSET);
        get_word(&cmd->file, cmd_line, end_exec - cmd_line); 

        char *end_cmd = strpbrk(cmd_line, "|\n");
        get_word(&cmd->arg, cmd_line, end_cmd - cmd_line); 

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
        cmd_alloc(cmd_arr, 10);

        while (true) {
                if ((cmd_line = get_cmd(&cmd_arr->ptr[cmd_count++], cmd_line)) == nullptr)
                        break;
                
                if (cmd_arr->cap < cmd_count + 1)
                        cmd_alloc(cmd_arr, cmd_arr->cap * 2);
        }

        cmd_arr->size = cmd_count;

        return 0;
}

int
run(cmd_arr_t *cmd_arr, int cmd_count)
{       
        cmd_t cmd = cmd_arr->ptr[cmd_count];

        int fds[2];
        pipe(fds);

        if (fork() == 0) {
                if (cmd_count < cmd_arr->size - 1) 
                        close(fds[1]);

                dup2(fds[0], 0);
                // Recursion there.
                if (cmd_count < cmd_arr->size - 1) {
                        run(cmd_arr, cmd_count + 1);
                }

                exit(EXIT_SUCCESS);
        } else {
                if (cmd_count < cmd_arr->size - 1) {
                        dup2(fds[1], 1);
                        close(fds[0]);
                }
                execl("/bin/sh", "/bin/sh", "-c", cmd.arg, NULL);

                wait(NULL);
                exit(EXIT_SUCCESS);
        }

        return 0;
}

void
cleanup(char *cmd_line, cmd_arr_t *cmd_arr)
{
        free(cmd_line);

        for (int i = 0; i < cmd_arr->size; i++) {
                free(cmd_arr->ptr[i].file);
                free(cmd_arr->ptr[i].arg);
        }

        free(cmd_arr->ptr);
}

