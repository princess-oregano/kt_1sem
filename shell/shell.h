#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>

const char* const BREAKSET = " |\n\t\r\f\v";

struct cmd_t {
        char *line = nullptr; 
        char **argv = nullptr; 
        int argc = 0;
        int cap = 0;
};

struct cmd_arr_t {
        cmd_t *ptr = nullptr;
        int cap = 0;
        int size = 0;
};

int
parser(char *cmd_line, size_t size, cmd_arr_t *cmd_arr);

int
run(cmd_arr_t *cmd_arr);

void
cleanup(char *cmd_line, cmd_arr_t *cmd_arr);

#endif // SHELL_H

