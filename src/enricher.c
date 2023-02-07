/* SPDX-License-Identifier: LGPL-3.0-or-later */

#include <error.h>
#include <string.h>
#include <sysexits.h>

#include "enricher.h"
#include "enrichers/internals.h"

struct rbh_iterator *
iter_enrich(struct rbh_iterator *fsevents, struct rbh_backend *backend,
            enum rbh_enricher_t enricher_type, int mount_fd)
{
    switch (enricher_type) {
#ifdef HAVE_LUSTRE
        case ENR_LUSTRE:
            return lustre_iter_enrich(backend, fsevents, mount_fd);
#endif
        case ENR_POSIX:
            return posix_iter_enrich(fsevents, mount_fd);
        default:
            error(EX_USAGE, EINVAL, "enricher type not allowed");
    }
    __builtin_unreachable();
}

enum rbh_enricher_t
parse_enricher_type(const char *arg)
{
    if (strcmp(arg, "posix") == 0)
        return ENR_POSIX;
    if (strcmp(arg, "lustre") == 0)
        return ENR_LUSTRE;
    error(EX_USAGE, EINVAL, "enricher type not allowed");
    __builtin_unreachable();
}
