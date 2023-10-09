#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell.h"

[[maybe_unused]] static void
print_cmd(cmd_arr_t cmd_arr) 
{
        for (int i = 0; i < cmd_arr.size; i++) {
                for (int j = 0; j < cmd_arr.ptr[i].argc; j++) {
                        fprintf(stderr, "'%s' ", cmd_arr.ptr[i].argv[j]);
                }
                fprintf(stderr, "\n");
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
        //print_cmd(cmd_arr);

        run(&cmd_arr, 0);

        cleanup(cmd_line, &cmd_arr);

        return 0;
}

