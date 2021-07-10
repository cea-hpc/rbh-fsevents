/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <error.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <robinhood/itertools.h>
#include <robinhood/fsevent.h>
#include <robinhood/ring.h>

#include "deduplicator.h"

static size_t pagesize;

static void __attribute__((constructor))
pagesize_init(void)
{
    pagesize = sysconf(_SC_PAGESIZE);
}

/*----------------------------------------------------------------------------*
 |                              ring_iter_new()                               |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                        dereference iterator                        |
     *--------------------------------------------------------------------*/

struct dereference_iterator {
    struct rbh_mut_iterator iterator;
    struct rbh_mut_iterator *subiter;
};

static void *
dereference_iter_next(void *iterator)
{
    struct dereference_iterator *dereference = iterator;
    void **pointer;

    pointer = rbh_mut_iter_next(dereference->subiter);
    if (pointer == NULL)
        return NULL;

    return *pointer;
}

static const struct rbh_mut_iterator_operations DEREFERENCE_ITER_OPS = {
    .next = dereference_iter_next,
    .destroy = free,
};

static const struct rbh_mut_iterator DEREFERENCE_ITERATOR = {
    .ops = &DEREFERENCE_ITER_OPS,
};

static struct rbh_mut_iterator *
dereference_iter_new(struct rbh_mut_iterator *iterator)
{
    struct dereference_iterator *dereference;

    dereference = malloc(sizeof(*dereference));
    if (dereference == NULL)
        return NULL;

    dereference->iterator = DEREFERENCE_ITERATOR;
    dereference->subiter = iterator;
    return &dereference->iterator;
}

static struct rbh_iterator *
ring_iter_new(struct rbh_ring *ring, size_t element_size)
{
    struct rbh_mut_iterator *iterator;
    struct rbh_mut_iterator *fsevents;
    struct rbh_iterator *wrapper;

    iterator = rbh_mut_iter_ring(ring, element_size);
    if (iterator == NULL)
        return NULL;

    fsevents = dereference_iter_new(iterator);
    if (fsevents == NULL) {
        int save_errno = errno;

        rbh_mut_iter_destroy(iterator);
        errno = save_errno;
        return NULL;
    }

    wrapper = rbh_iter_constify(fsevents);
    if (wrapper == NULL) {
        int save_errno = errno;

        rbh_mut_iter_destroy(fsevents);
        errno = save_errno;
        return NULL;
    }

    return wrapper;
}

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
    struct rbh_ring *ring;
    bool exhausted;

    const struct rbh_fsevent *fsevent;
};

static void *
deduplicator_iter_next(void *iterator)
{
    struct deduplicator *deduplicator = iterator;

    while (!deduplicator->exhausted) {
        void *fsevent;

        deduplicator->fsevent = rbh_iter_next(&deduplicator->source->fsevents);
        if (deduplicator->fsevent == NULL) {
            int save_errno = errno;

            if (save_errno != ENODATA) {
                errno = save_errno;
                return NULL;
            }

            deduplicator->exhausted = true;
            return ring_iter_new(deduplicator->ring, sizeof(fsevent));
        }

        fsevent = fsevent_clone(deduplicator->fsevent);
        if (fsevent == NULL)
            return NULL;

        if (!rbh_ring_push(deduplicator->ring, &fsevent, sizeof(fsevent))) {
            assert(errno == ENOBUFS);
            return ring_iter_new(deduplicator->ring, sizeof(fsevent));
        }
    }

    errno = ENODATA;
    return NULL;
}

static void
deduplicator_iter_destroy(void *iterator)
{
    struct deduplicator *deduplicator = iterator;
    struct rbh_fsevent **fsevents;
    size_t readable;

    fsevents = rbh_ring_peek(deduplicator->ring, &readable);
    assert(readable % sizeof(*fsevents) == 0);

    for (size_t i = 0; i < readable / sizeof(*fsevents); i++)
        free(fsevents[i]);

    rbh_ring_destroy(deduplicator->ring);
    free(deduplicator);
}

static const struct rbh_mut_iterator_operations DEDUPLICATOR_ITER_OPS = {
    .next = deduplicator_iter_next,
    .destroy = deduplicator_iter_destroy,
};

static const struct rbh_mut_iterator DEDUPLICATOR_ITERATOR = {
    .ops = &DEDUPLICATOR_ITER_OPS,
};

static size_t
_ceil(size_t num, size_t unit)
{
    return num + (unit - num % unit);
}

struct rbh_mut_iterator *
deduplicator_new(size_t count, struct source *source)
{
    struct deduplicator *deduplicator;

    deduplicator = malloc(sizeof(*deduplicator));
    if (deduplicator == NULL)
        return NULL;

    deduplicator->ring = rbh_ring_new(_ceil(sizeof(void *) * count, pagesize));
    if (deduplicator->ring == NULL) {
        int save_errno = errno;

        free(deduplicator);
        errno = save_errno;
        return NULL;
    }

    deduplicator->batches = DEDUPLICATOR_ITERATOR;
    deduplicator->source = source;
    deduplicator->exhausted = false;
    return &deduplicator->batches;
}
