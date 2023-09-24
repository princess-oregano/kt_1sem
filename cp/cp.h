#ifndef CP_H
#define CP_H

#include <getopt.h>

const int BUF_SIZE = 4096;
const option long_options[] = {
        {"force", no_argument, 0, 'f'},
        {"interactive", no_argument, 0, 'i'},
        {"verbose", no_argument, 0, 'v'},
        {"preserve", no_argument, 0, 'p'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
};

int
parse(int argc, char *argv[]);

int
cp(const char *src, const char *dest);

#endif // CP_H
