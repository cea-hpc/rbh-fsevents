#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <error.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include <robinhood.h>
#include <robinhood/utils.h>

static void
usage(void)
{
    const char *message =
        "usage: %s [-h] [--raw] [--enrich MOUNTPOINT] SOURCE DESTINATION\n"
        "\n"
        "Collect changelog records from SOURCE, optionally enrich them with data\n"
        "collected from MOUNTPOINT and send them to DESTINATION.\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE          can be one of:\n"
        "                        a path to a yaml file, or '-' for stdin;\n"
        "                        an MDT name (eg. lustre-MDT0000).\n"
        "    DESTINATION     can be one of:\n"
        "                        '-' for stdout;\n"
        "                        a RobinHood URI (eg. rbh:mongo:test).\n"
        "\n"
        "Optional arguments:\n"
        "    -h, --help      print this message and exit\n"
        "    -r, --raw       do not enrich changelog records (default)\n"
        "    -e, --enrich MOUNTPOINT\n"
        "                    enrich changelog records by querying MOUNTPOINT as needed\n"
        "\n"
        "Note that uploading raw records to a RobinHood backend will fail, they have to\n"
        "be enriched first.\n";

    printf(message, program_invocation_short_name);
}

static struct rbh_backend *backend;

static void __attribute__((destructor))
backend_exit(void)
{
    if (backend)
        rbh_backend_destroy(backend);
}

static const char *mountpoint;

static struct rbh_iterator *
fsevents_from_file(FILE *file)
{
    if (fclose(file) == EOF)
        error(EXIT_FAILURE, errno, "fclose");

    /* TODO: parse yaml documents and enrich them if needed/possible */

    error(EXIT_FAILURE, ENOSYS, "%s", __func__);
    __builtin_unreachable();
}

static struct rbh_iterator *
fsevents_from_source(const char *arg)
{
    FILE *file;

    if (strcmp(arg, "-") == 0)
        /* SOURCE is '-' (stdin) */
        return fsevents_from_file(stdin);

    file = fopen(arg, "r");
    if (file != NULL)
        /* SOURCE is a path to a file */
        return fsevents_from_file(file);
    if (file == NULL && errno != ENOENT)
        /* SOURCE is a path to a file, but there was some sort of error trying
         * to open it.
         */
        error(EXIT_FAILURE, errno, "%s", arg);

    /* TODO: parse SOURCE as an MDT name (ie. <fsname>-MDT<index>) */
    error(EXIT_FAILURE, EINVAL, "%s", arg);
    __builtin_unreachable();
}

int
main(int argc, char *argv[])
{
    const struct option LONG_OPTIONS[] = {
        {
            .name = "enrich",
            .has_arg = required_argument,
            .val = 'e',
        },
        {
            .name = "help",
            .val = 'h',
        },
        {
            .name = "raw",
            .val = 'r',
        },
        {}
    };
    struct rbh_iterator *fsevents;
    char c;

    /* Parse the command line */
    while ((c = getopt_long(argc, argv, "e:hr", LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
        case 'e':
            mountpoint = optarg;
            break;
        case 'h':
            usage();
            return 0;
        case 'r':
            mountpoint = NULL;
            break;
        case '?':
        default:
            /* getopt_long() prints meaningful error messages itself */
            exit(EX_USAGE);
        }
    }

    if (argc - optind < 2)
        error(EX_USAGE, 0, "not enough arguments");
    if (argc - optind > 2)
        error(EX_USAGE, 0, "too many arguments");

    fsevents = fsevents_from_source(argv[optind++]);

    /* TODO: parse DESTINATION and feed it SOURCE's fsevents */
    rbh_iter_destroy(fsevents);

    error(EXIT_FAILURE, ENOSYS, "%s", __func__);
}
