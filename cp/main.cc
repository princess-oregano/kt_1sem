#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include "cp.h"

int
main(int argc, char *argv[])
{
        parse(argc, argv);

        return cp(argv[optind], argv[optind + 1]);
}

