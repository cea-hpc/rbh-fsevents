/* SPDX-License-Identifier: LGPL-3.0-or-later */

#include <error.h>
#include <string.h>
#include <sysexits.h>

#include "enricher.h"
#include "enrichers/internals.h"

struct enrich *
enrich_from_backend(struct rbh_backend *backend, const char *mount_path)
{
    switch (backend->id) {
        case RBH_BI_POSIX:
            return posix_enrich_from_backend(backend, mount_path);
#ifdef HAVE_LUSTRE
        case RBH_BI_LUSTRE:
            return lustre_enrich_from_backend(backend, mount_path);
#endif
        default:
            error(EX_USAGE, EINVAL, "enricher type not allowed");
    }
    __builtin_unreachable();
}
