#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include "cp.h"

int
main(int argc, char *argv[])
{
        parse(argc, argv);

        cp(argv[optind], argv[optind + 1]);

        return 0;
}

