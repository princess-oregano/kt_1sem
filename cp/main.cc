#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include "cp.h"

int
main(int argc, char *argv[])
{
        parse(argc, argv);

        int n = argc - optind;
        if (n < 2) {
                fprintf(stderr, "cp: missing destination file operand"
                                "after '%s'\n", argv[optind]);
                return 1;
        } else if (n == 2) {            // File to file copy.
                if (!check_dir(argv[optind + 1])) {
                        return cp(argv[optind], argv[optind + 1]);
                } else {
                        char *new_name = name_wdir(argv[optind], argv[optind + 1]);
                        fprintf(stderr, "%s\n", new_name);
                        cp(argv[optind], new_name);
                        free(new_name);
                }
        } else if (n > 2) {             // Files to dir copy.
                if (!check_dir(argv[argc - 1])) {
                        fprintf(stderr, "cp: target '%s': "
                                        "No such file or directory", argv[argc - 1]);
                        return 1;
                }

                for (int i = optind; i < argc - 1; i++) {
                        char *new_name = name_wdir(argv[i], argv[argc - 1]);
                        fprintf(stderr, "%s\n", new_name);
                        cp(argv[i], new_name);
                        free(new_name);
                }
        }
}

