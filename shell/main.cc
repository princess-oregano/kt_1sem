#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
        while (true) {
                char *cmd_line = nullptr;

                printf(" $ ");
                size_t n = getline(&cmd_line, &n, stdin);

                if (!strcmp(cmd_line, "quit\n")) {
                        free(cmd_line);
                        break;
                }

                cmd_arr_t cmd_arr;
                parser(cmd_line, n, &cmd_arr);

                print_cmd(cmd_arr);

                free(cmd_line);
        }
        
        return 0;
}

