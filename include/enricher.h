/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef ENRICHER_H
#define ENRICHER_H

#include <robinhood/iterator.h>

struct enrich;

struct enrich_operations {
    struct rbh_iterator *(*process)(void *enrich,
                                    struct rbh_iterator *fsevents);
    void (*destroy)(void *enrich);
};

struct enrich {
    const char *name;
    const struct enrich_operations *ops;
};

static inline struct rbh_iterator *
enrich_process(struct enrich *enrich, struct rbh_iterator *fsevents)
{
    return enrich->ops->process(enrich, fsevents);
}

static inline void
enrich_destroy(struct enrich *enrich)
{
    return enrich->ops->destroy(enrich);
}

struct enrich *
enrich_from_backend(struct rbh_backend *rbh_backend, const char *mount_path);

struct rbh_iterator *
iter_no_partial(struct rbh_iterator *fsevents);

#endif
