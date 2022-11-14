/* SPDX-License-Identifier: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>

#include <lustre/lustreapi.h>

#include <robinhood/fsevent.h>
#include <robinhood/sstack.h>
#include <robinhood/statx.h>

#include "source.h"

static void
values_flush(struct rbh_sstack *values)
{
    while (true) {
        size_t readable;

        rbh_sstack_peek(values, &readable);
        if (readable == 0)
            break;

        rbh_sstack_pop(values, readable);
    }
    rbh_sstack_shrink(values);
}

struct lustre_changelog_iterator {
    struct rbh_iterator iterator;

    struct rbh_sstack *values;
    void *reader;
    struct changelog_rec *prev_record;
    unsigned int process_step;
};

static void *
fsevent_from_record(struct changelog_rec *record)
{
    struct rbh_fsevent *bad;

    (void)record;

    bad = malloc(sizeof(*bad));
    if (bad == NULL)
        return NULL;

    bad->type = -1;
    bad->id.data = NULL;
    bad->id.size = 0;

    return bad;
}

static struct rbh_value_pair ENRICH_PAIR[] = {
    { .key = "statx" },
};

static const struct rbh_value ENRICH_MAP = {
    .type = RBH_VT_MAP,
    .map = {
        .pairs = ENRICH_PAIR,
        .count = 1,
    },
};

static struct rbh_value_pair NS_XATTRS_ENRICH_PAIR[] = {
    { .key = "xattrs" },
};

static const struct rbh_value NS_XATTRS_ENRICH_MAP = {
    .type = RBH_VT_MAP,
    .map = {
        .pairs = NS_XATTRS_ENRICH_PAIR,
        .count = 1,
    },
};

static struct rbh_value_pair NS_XATTRS_PAIRS[] = {
    { .key = "rbh-fsevents", .value = &NS_XATTRS_ENRICH_MAP },
};

static const struct rbh_value_map NS_XATTRS_MAP = {
    .pairs = NS_XATTRS_PAIRS,
    .count = 1,
};

static struct rbh_value_pair FID_XATTRS_PAIRS[] = {
    { .key = "fid" },
 };

static const struct rbh_value_map FID_XATTRS_MAP = {
    .pairs = FID_XATTRS_PAIRS,
    .count = 1,
};

static const struct rbh_value_pair XATTRS_PAIRS[] = {
    { .key = "rbh-fsevents", .value = &ENRICH_MAP },
};

static const struct rbh_value_map XATTRS_MAP = {
    .pairs = XATTRS_PAIRS,
    .count = 1,
};

/* BSON results:
 * { "statx" : { "uid" : x, "gid" : y } }
 */
static void
fill_uidgid(struct changelog_rec *record, struct rbh_statx *statx)
{
    struct changelog_ext_uidgid *uidgid;

    statx->stx_mask |= RBH_STATX_UID | RBH_STATX_GID;
    uidgid = changelog_rec_uidgid(record);
    statx->stx_uid = uidgid->cr_uid;
    statx->stx_gid = uidgid->cr_gid;
}

/* BSON results:
 * { "ns" : [ { "xattrs": { "fid" : x } } ] }
 */
static int
fill_ns_xattrs_fid(struct rbh_sstack *values, struct changelog_rec *record,
                   struct rbh_value_pair *pair)
{
    struct rbh_value lu_fid_value;

    lu_fid_value.type = RBH_VT_BINARY;
    lu_fid_value.binary.size = sizeof(record->cr_tfid);
    lu_fid_value.binary.data = rbh_sstack_push(values,
                                               (const char *)&record->cr_tfid,
                                               sizeof(record->cr_tfid));
    if (lu_fid_value.binary.data == NULL)
        return -1;

    pair->value = rbh_sstack_push(values, &lu_fid_value, sizeof(lu_fid_value));
    if (pair->value == NULL)
        return -1;

    return 0;
}

static int
fill_enrich_mask(uint32_t enrich_mask, struct rbh_sstack *values,
                 struct rbh_value_pair *pair)
{
    struct rbh_value *enrich_mask_value =
        rbh_sstack_push(values, NULL, sizeof(*enrich_mask_value));

    if (enrich_mask_value == NULL)
        return -1;

    enrich_mask_value->type = RBH_VT_UINT32;
    enrich_mask_value->uint32 = enrich_mask;

    pair->value = enrich_mask_value;

    return 0;
}

static const void *
lustre_changelog_iter_next(void *iterator)
{
    struct lustre_changelog_iterator *records = iterator;
    struct rbh_fsevent *fsevent = NULL;
    uint32_t statx_enrich_mask = 0;
    struct changelog_rec *record;
    struct rbh_statx *rec_statx;
    struct rbh_id *tmp_id;
    struct rbh_id *id;
    size_t data_size;
    int save_errno;
    char *data;
    int rc;

    values_flush(records->values);

retry:
    if (records->prev_record == NULL) {
        rc = llapi_changelog_recv(records->reader, &record);
        if (rc < 0) {
            errno = -rc;
            return NULL;
        }
        if (rc > 0) {
            errno = ENODATA;
            return NULL;
        }
    } else {
        record = records->prev_record;
        records->prev_record = NULL;
    }

    tmp_id = rbh_id_from_lu_fid(&record->cr_tfid);
    if (tmp_id == NULL)
        goto end_event;

    data_size = tmp_id->size;
    id = rbh_sstack_push(records->values, NULL, sizeof(*id) + data_size);
    if (id == NULL)
        goto end_event;

    data = (char *)(id + 1);
    rc = rbh_id_copy(id, tmp_id, &data, &data_size);
    if (rc)
        goto end_event;

    free(tmp_id);

    rec_statx = rbh_sstack_push(records->values, NULL, sizeof(*rec_statx));
    if (rec_statx == NULL)
        goto end_event;

    rec_statx->stx_mask = 0;

    fsevent = rbh_sstack_push(records->values, NULL, sizeof(*fsevent));
    if (fsevent == NULL)
        goto end_event;
    memset(fsevent, 0, sizeof(*fsevent));
    fsevent->id.data = id->data;
    fsevent->id.size = id->size;

    switch(record->cr_type) {
    case CL_CREATE:
        assert(records->process_step < 2);
        switch(records->process_step) {
            case 0:
                fill_uidgid(record, rec_statx);

                fsevent->type = RBH_FET_LINK;
                fsevent->xattrs = NS_XATTRS_MAP;

                if (fill_enrich_mask(RBH_XATTRS_PATH, records->values,
                                     &NS_XATTRS_ENRICH_PAIR[0]))
                    goto err;

                tmp_id = rbh_id_from_lu_fid(&record->cr_pfid);
                if (tmp_id == NULL)
                    goto err;

                fsevent->link.parent_id = tmp_id;
                fsevent->link.name = changelog_rec_name(record);
                goto save_event;
            case 1:
                fsevent->type = RBH_FET_XATTR;
                fsevent->xattrs = FID_XATTRS_MAP;
                if (fill_ns_xattrs_fid(records->values, record,
                                       &FID_XATTRS_PAIRS[0]))
                    goto err;

                records->process_step = 0;
                goto end_event;
        }
        __builtin_unreachable();
    case CL_CLOSE:
        enrich_mask |= RBH_STATX_ATIME_SEC | RBH_STATX_ATIME_NSEC;
        if (fill_enrich_mask(statx_enrich_mask, records->values,
                             &ENRICH_PAIR[0]))
            goto err;

        fsevent->type = RBH_FET_UPSERT;
        goto end_event;
    case CL_MKDIR:      /* RBH_FET_UPSERT */
        fsevent->type = RBH_FET_UPSERT;

        if (fill_enrich_mask(statx_enrich_mask, records->values,
                             &ENRICH_PAIR[0]))
            goto err;

        goto end_event;
    case CL_HARDLINK:   /* RBH_FET_LINK? */
    case CL_SOFTLINK:   /* RBH_FET_UPSERT + symlink */
    case CL_MKNOD:
    case CL_UNLINK:     /* RBH_FET_UNLINK or RBH_FET_DELETE */
    case CL_RMDIR:      /* RBH_FET_UNLINK or RBH_FET_DELETE */
    case CL_RENAME:     /* RBH_FET_UPSERT */
    case CL_EXT:
    case CL_OPEN:
    case CL_LAYOUT:
    case CL_TRUNC:
    case CL_SETATTR:    /* RBH_FET_XATTR? */
    case CL_SETXATTR:   /* RBH_FET_XATTR */
    case CL_HSM:
    case CL_MTIME:
    case CL_ATIME:
    case CL_CTIME:
    case CL_MIGRATE:
    case CL_FLRW:
    case CL_RESYNC:
    case CL_GETXATTR:
    case CL_DN_OPEN:
        fsevent = fsevent_from_record(record);
        goto end_event;
    default: /* CL_MARK or other events */
        /* Events not managed yet, we go to the next record */
        llapi_changelog_free(&record);
        goto retry;
    }

    __builtin_unreachable();

err:
    fsevent = NULL;

end_event:
    save_errno = errno;
    llapi_changelog_free(&record);
    errno = save_errno;

    return fsevent;

save_event:
    records->prev_record = record;
    ++records->process_step;

    return fsevent;
}

static void
lustre_changelog_iter_destroy(void *iterator)
{
    struct lustre_changelog_iterator *records = iterator;

    rbh_sstack_destroy(records->values);
    llapi_changelog_fini(&records->reader);
}

static const struct rbh_iterator_operations LUSTRE_CHANGELOG_ITER_OPS = {
    .next = lustre_changelog_iter_next,
    .destroy = lustre_changelog_iter_destroy,
};

static const struct rbh_iterator LUSTRE_CHANGELOG_ITERATOR = {
    .ops = &LUSTRE_CHANGELOG_ITER_OPS,
};

static void
lustre_changelog_init(struct lustre_changelog_iterator *events,
                      const char *mdtname)
{
    int rc;

    rc = llapi_changelog_start(&events->reader,
                               CHANGELOG_FLAG_JOBID |
                               CHANGELOG_FLAG_EXTRA_FLAGS,
                               mdtname, 0 /*start_rec*/);
    if (rc < 0)
        error(EXIT_FAILURE, -rc, "llapi_changelog_start");

    rc = llapi_changelog_set_xflags(events->reader,
                                    CHANGELOG_EXTRA_FLAG_UIDGID |
                                    CHANGELOG_EXTRA_FLAG_NID |
                                    CHANGELOG_EXTRA_FLAG_OMODE |
                                    CHANGELOG_EXTRA_FLAG_XATTR);
    if (rc < 0)
        error(EXIT_FAILURE, -rc, "llapi_changelog_set_xflags");

    events->iterator = LUSTRE_CHANGELOG_ITERATOR;
}

struct lustre_source {
    struct source source;

    struct lustre_changelog_iterator events;
};

static const void *
source_iter_next(void *iterator)
{
    struct lustre_source *source = iterator;

    return rbh_iter_next(&source->events.iterator);
}

static void
source_iter_destroy(void *iterator)
{
    struct lustre_source *source = iterator;

    rbh_iter_destroy(&source->events.iterator);
    free(source);
}

static const struct rbh_iterator_operations SOURCE_ITER_OPS = {
    .next = source_iter_next,
    .destroy = source_iter_destroy,
};

static const struct source LUSTRE_SOURCE = {
    .name = "lustre",
    .fsevents = {
        .ops = &SOURCE_ITER_OPS,
    },
};

struct source *
source_from_lustre_changelog(const char *mdtname)
{
    struct lustre_source *source;

    source = malloc(sizeof(*source));
    if (source == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    lustre_changelog_init(&source->events, mdtname);

    source->events.values = rbh_sstack_new(sizeof(struct rbh_value_pair) *
                                           (1 << 7));
    source->events.prev_record = NULL;
    source->source = LUSTRE_SOURCE;
    return &source->source;
}
