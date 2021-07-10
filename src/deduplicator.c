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
#include "record.h"

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
     |                      records_iter_constify()                       |
     *--------------------------------------------------------------------*/

struct records_constify_iterator {
    struct rbh_iterator iterator;

    struct rbh_mut_iterator *records;
    struct record *record;
};

static const void *
records_constify_iter_next(void *iterator)
{
    struct records_constify_iterator *constify = iterator;

    if (constify->record)
        free(constify->record->fsevent);
    constify->record = rbh_mut_iter_next(constify->records);
    return constify->record;
}

static void
records_constify_iter_destroy(void *iterator)
{
    struct records_constify_iterator *constify = iterator;

    if (constify->record)
        free(constify->record->fsevent);
    rbh_mut_iter_destroy(constify->records);
    free(constify);
}

static const struct rbh_iterator_operations RECORDS_CONSTIFY_ITER_OPS = {
    .next = records_constify_iter_next,
    .destroy = records_constify_iter_destroy,
};

static const struct rbh_iterator RECORDS_CONSTIFY_ITERATOR = {
    .ops = &RECORDS_CONSTIFY_ITER_OPS,
};

struct rbh_iterator *
records_iter_constify(struct rbh_mut_iterator *records)
{
    struct records_constify_iterator *constify;

    constify = malloc(sizeof(*constify));
    if (constify == NULL)
        return NULL;

    constify->iterator = RECORDS_CONSTIFY_ITERATOR;
    constify->records = records;
    constify->record = NULL;
    return &constify->iterator;
}

/*----------------------------- ring_iter_new() ------------------------------*/

static struct rbh_iterator *
ring_iter_new(struct rbh_ring *ring, size_t element_size)
{
    struct rbh_mut_iterator *records;
    struct rbh_iterator *wrapper;

    records = rbh_mut_iter_ring(ring, element_size);
    if (records == NULL)
        return NULL;

    wrapper = records_iter_constify(records);
    if (wrapper == NULL) {
        int save_errno = errno;

        rbh_mut_iter_destroy(records);
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

    if (deduplicator->fsevent)
        goto record_fsevent;

    while (!deduplicator->exhausted) {
        struct record record;

        deduplicator->fsevent = rbh_iter_next(&deduplicator->source->fsevents);
        if (deduplicator->fsevent == NULL) {
            int save_errno = errno;

            if (save_errno != ENODATA) {
                errno = save_errno;
                return NULL;
            }

            deduplicator->exhausted = true;
            return ring_iter_new(deduplicator->ring, sizeof(record));
        }

record_fsevent:
        record.fsevent = fsevent_clone(deduplicator->fsevent);
        if (record.fsevent == NULL)
            return NULL;

        if (!rbh_ring_push(deduplicator->ring, &record, sizeof(record))) {
            assert(errno == ENOBUFS);
            free(record.fsevent);
            return ring_iter_new(deduplicator->ring, sizeof(record));
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
    deduplicator->fsevent = NULL;
    return &deduplicator->batches;
}
