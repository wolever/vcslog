#include <stdlib.h>
#include <stdio.h>

#include "xquotestr.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s test_string", argv[0]);
        exit(1);
    }

    char output[1024];
    puts(xquotestr(argv[1], output, sizeof(output)));
    return 0;
}
