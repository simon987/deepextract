#include <stdio.h>
#include "thirdparty/argparse/argparse.h"
#include "extract.h"

#define DESCRIPTION ""
#define EPILOG "Made by simon987 <me@simon987.net>. Released under GPL-3.0"


static const char *const Version = "1.0";
static const char *const usage[] = {
        "deepextract [OPTION]... SOURCE DESTINATION",
        NULL,
};

int validate_args(args_t *args) {

    if (args->dry_run != 0) {
        args->verbose = TRUE;
    }

    if (args->threads <= 0) {
        fprintf(stderr, "Invalid thread count");
        return FALSE;
    }
    return TRUE;
}

int main(int argc, const char **argv) {

    args_t args = {0, 0, 0, 1};

    struct argparse_option options[] = {
            OPT_HELP(),

            OPT_BOOLEAN('v', "version", &args.version, "Show version and exit"),
            OPT_BOOLEAN(0, "verbose", &args.verbose, "Turn on logging"),
            OPT_BOOLEAN(0, "dry-run", &args.dry_run, "Don't modify filesystem (implies --verbose)"),

            OPT_GROUP("Options"),
            OPT_INTEGER('t', "threads", &args.threads, "Thread count"),

            OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, usage, 0);
    argparse_describe(&argparse, DESCRIPTION, EPILOG);
    argc = argparse_parse(&argparse, argc, argv);

    if (args.version) {
        printf(Version);
        return 0;
    }

    if (!validate_args(&args)) {
        return -1;
    }

    return extract(&args);
}
