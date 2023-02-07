/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef ENRICHER_H
#define ENRICHER_H

#include <robinhood/id.h>
#include <robinhood/iterator.h>

struct rbh_iterator *
iter_enrich(struct rbh_iterator *fsevents, int mount_fd);

struct rbh_iterator *
iter_no_partial(struct rbh_iterator *fsevents);

void
parse_enricher_type(const char *arg);

#endif
