/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef ENRICHER_INTERNALS_H
#define ENRICHER_INTERNALS_H

#include <robinhood.h>

struct enricher {
    struct rbh_iterator iterator;
    struct rbh_backend *backend;

    struct rbh_iterator *fsevents;
    int mount_fd;

    struct rbh_value_pair *pairs;
    size_t pair_count;

    struct rbh_fsevent fsevent;
    struct rbh_statx statx;
    char *symlink;
};

int
open_by_id(int mound_fd, const struct rbh_id *id, int flags);

/*----------------------------------------------------------------------------*
 *                              posix internals                               *
 *----------------------------------------------------------------------------*/

int posix_enrich(const struct rbh_value_pair *partial,
                 struct rbh_value_pair **pairs, size_t *pair_count,
                 struct rbh_fsevent *enriched,
                 const struct rbh_fsevent *original, int mount_fd,
                 struct rbh_statx *statxbuf, char symlink[]);

struct rbh_iterator *
posix_iter_enrich(struct rbh_iterator *fsevents, int mount_fd);

void
posix_enricher_iter_destroy(void *iterator);

void
posix_backend_enrich_destroy(void *_enrich);

/*----------------------------------------------------------------------------*
 *                            enricher interfaces                             *
 *----------------------------------------------------------------------------*/

struct backend_enrich {
    struct enrich enrich;
    struct rbh_backend *backend;
    int mount_fd;
};

struct enrich *
posix_enrich_from_backend(struct rbh_backend *backend, const char *mount_path);

#ifdef HAVE_LUSTRE
struct enrich *
lustre_enrich_from_backend(struct rbh_backend *backend, const char *mount_path);
#endif

#endif
