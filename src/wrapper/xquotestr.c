#include <stdlib.h>
#include <stdio.h>

#include "bool.h"

/**
 * Quote 'input', writing to 'output' and returning a pointer to the start of
 * the result (may be `offset + 1`).
 * Quoting rules are:
 * - '"', '\' and '\n' will be escaped with a '\'.
 * - Double quotes ('"') will be added around the string if it contains
 *   '[ \n\t"\\]'.
 */
char *xquotestr(const char *input, char *output, size_t size) {
    bool needs_quotes = false;
    size_t ioff = 0;
    size_t ooff = 1;
    while (input[ioff]) {
        char ch = input[ioff++];
        switch (ch) {
            case '\n':
                ch = 'n';
            case '"':
            case '\\':
                output[ooff++] = '\\';
            case ' ':
            case '\t':
                needs_quotes = true;
                break;
        }
        output[ooff++] = ch;
        if (ooff >= size - 2) {
            output[(size > 10)? 10 : size] = '\0';
            fprintf(stderr, "vcslog-wrapper: xquotestr failed "
                            "(not enough space in output for %s...)",
                            output + 1);
            exit(1);
        }
    }

    if (needs_quotes) {
        output[0] = '"';
        output[ooff++] = '"';
        output[ooff++] = '\0';
        return output;
    }
    output[ooff++] = '\0';
    return output + 1;
}
