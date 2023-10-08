#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell.h"

static void
print_cmd(cmd_arr_t cmd_arr) 
{
        for (int i = 0; i < cmd_arr.size; i++) {
                fprintf(stderr, "%s %s\n", cmd_arr.ptr[i].file, cmd_arr.ptr[i].arg);
        }
}

int
main()
{
        char *cmd_line = nullptr;

        printf(" $ ");
        size_t n = getline(&cmd_line, &n, stdin);

        cmd_arr_t cmd_arr;
        parser(cmd_line, n, &cmd_arr);

        cmd_t cmd = cmd_arr.ptr[0];

        run(&cmd_arr, 0);

        cleanup(cmd_line, &cmd_arr);
        
        return 0;
}

