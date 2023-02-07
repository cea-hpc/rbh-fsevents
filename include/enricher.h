/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef ENRICHER_H
#define ENRICHER_H

#include <robinhood/backend.h>
#include <robinhood/iterator.h>

enum rbh_enricher_t {
    ENR_POSIX = 0,
    ENR_LUSTRE
};

struct rbh_iterator *
iter_enrich(struct rbh_iterator *fsevents, struct rbh_backend *backend,
            enum rbh_enricher_t enricher_type, int mount_fd);

struct rbh_iterator *
iter_no_partial(struct rbh_iterator *fsevents);

enum rbh_enricher_t
parse_enricher_type(const char *arg);

#endif
