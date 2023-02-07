/* SPDX-License-Identifier: LGPL-3.0-or-later */

#include <error.h>
#include <string.h>
#include <sysexits.h>

#include "enricher.h"
#include "enrichers/internals.h"

enum rbh_enricher_t {
    ENR_POSIX = 0,
    ENR_LUSTRE
};

static enum rbh_enricher_t enricher_type = ENR_POSIX;

struct rbh_iterator *
iter_enrich(struct rbh_iterator *fsevents, int mount_fd)
{
    switch (enricher_type) {
#ifdef HAVE_LUSTRE
        case ENR_LUSTRE:
            return lustre_iter_enrich(fsevents, mount_fd);
#endif
        case ENR_POSIX:
            return posix_iter_enrich(fsevents, mount_fd);
        default:
            error(EX_USAGE, EINVAL, "enricher type not allowed");
    }
}

void
parse_enricher_type(const char *arg)
{
    if (strcmp(arg, "posix") == 0)
        enricher_type = ENR_POSIX;
    if (strcmp(arg, "lustre") == 0)
        enricher_type = ENR_LUSTRE;
    error(EX_USAGE, EINVAL, "enricher type not allowed");
}
