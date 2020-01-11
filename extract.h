#ifndef DEEPEXTRACT_EXTRACT_H
#define DEEPEXTRACT_EXTRACT_H

#include "tpool.h"

#define TRUE 1
#define FALSE 0
#define _XOPEN_SOURCE 500

typedef struct args {
    int version;
    int verbose;
    int dry_run;
    int threads;
} args_t;

int extract(args_t *args);

#endif
