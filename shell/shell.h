#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>

const char* const BREAKSET = " |\n\t\r\f\v\0";

struct cmd_t {
        char *file = nullptr;
        char *arg  = nullptr; 
};

struct cmd_arr_t {
        cmd_t *ptr = nullptr;
        int cap = 0;
        int size = 0;
};

int
parser(char *cmd_line, size_t size, cmd_arr_t *cmd_arr);

#endif // SHELL_H

