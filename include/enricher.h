/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef ENRICHER_H
#define ENRICHER_H

#include <robinhood/id.h>
#include <robinhood/iterator.h>

struct rbh_iterator *
posix_iter_enrich(struct rbh_iterator *fsevents, int mount_fd);

#ifdef HAVE_LUSTRE
struct rbh_iterator *
lustre_iter_enrich(struct rbh_iterator *fsevents, int mount_fd);
#endif

struct rbh_iterator *
iter_no_partial(struct rbh_iterator *fsevents);

#endif
