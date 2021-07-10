/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <error.h>
#include <stdlib.h>

#include <robinhood/itertools.h>
#include <robinhood/fsevent.h>
#include <robinhood/ring.h>

#include "deduplicator.h"
#include "record.h"

/*----------------------------------------------------------------------------*
 |                               fsevent_clone                                |
 *----------------------------------------------------------------------------*/

static struct rbh_fsevent *
fsevent_clone(const struct rbh_fsevent *fsevent)
{
    switch (fsevent->type) {
    case RBH_FET_UPSERT:
        return rbh_fsevent_upsert_new(&fsevent->id, &fsevent->xattrs,
                                      fsevent->upsert.statx,
                                      fsevent->upsert.symlink);
    case RBH_FET_LINK:
        return rbh_fsevent_link_new(&fsevent->id, &fsevent->xattrs,
                                    fsevent->link.parent_id,
                                    fsevent->link.name);
    case RBH_FET_UNLINK:
        return rbh_fsevent_unlink_new(&fsevent->id, fsevent->link.parent_id,
                                      fsevent->link.name);
    case RBH_FET_DELETE:
        return rbh_fsevent_delete_new(&fsevent->id);
    case RBH_FET_XATTR:
        if (fsevent->ns.parent_id == NULL) {
            assert(fsevent->ns.name == NULL);
            return rbh_fsevent_xattr_new(&fsevent->id, &fsevent->xattrs);
        }
        assert(fsevent->ns.name);
        return rbh_fsevent_ns_xattr_new(&fsevent->id, &fsevent->xattrs,
                                        fsevent->ns.parent_id,
                                        fsevent->ns.name);
    }
    error(EXIT_FAILURE, 0, "unexpected fsevent type: %i", fsevent->type);
    __builtin_unreachable();
}

/*----------------------------------------------------------------------------*
 |                                deduplicator                                |
 *----------------------------------------------------------------------------*/

struct deduplicator {
    struct rbh_mut_iterator batches;

    struct source *source;
    struct record record;
};

static void *
deduplicator_iter_next(void *iterator)
{
    struct deduplicator *deduplicator = iterator;
    const struct rbh_fsevent *fsevent;

    fsevent = rbh_iter_next(&deduplicator->source->fsevents);
    if (fsevent == NULL)
        return NULL;

    if (deduplicator->record.fsevent)
        free(deduplicator->record.fsevent);

    deduplicator->record.fsevent = fsevent_clone(fsevent);
    if (deduplicator->record.fsevent == NULL)
        /* FIXME: we are losing track of the fsevent on error */
        return NULL;

    return rbh_iter_array(&deduplicator->record, sizeof(deduplicator->record),
                          1);
}

static void
deduplicator_iter_destroy(void *iterator)
{
    struct deduplicator *deduplicator = iterator;

    if (deduplicator->record.fsevent)
        free(deduplicator->record.fsevent);
    free(deduplicator);
}

static const struct rbh_mut_iterator_operations DEDUPLICATOR_ITER_OPS = {
    .next = deduplicator_iter_next,
    .destroy = deduplicator_iter_destroy,
};

static const struct rbh_mut_iterator DEDUPLICATOR_ITERATOR = {
    .ops = &DEDUPLICATOR_ITER_OPS,
};

struct rbh_mut_iterator *
deduplicator_new(size_t count __attribute__((unused)), struct source *source)
{
    struct deduplicator *deduplicator;

    deduplicator = malloc(sizeof(*deduplicator));
    if (deduplicator == NULL)
        return NULL;

    deduplicator->batches = DEDUPLICATOR_ITERATOR;
    deduplicator->source = source;
    deduplicator->record.fsevent = NULL;
    return &deduplicator->batches;
}
