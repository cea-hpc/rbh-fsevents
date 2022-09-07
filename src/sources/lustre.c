/* SPDX-License-Identifier: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>

#include <lustre/lustreapi.h>

#include <robinhood/fsevent.h>
#include <robinhood/sstack.h>
#include <robinhood/statx.h>

#include "source.h"

struct lustre_changelog_iterator {
    struct rbh_iterator iterator;

    struct rbh_sstack *values;
    void *reader;
};

static const void *
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

static struct rbh_value NS_XATTRS_MAP = {
    .type = RBH_VT_MAP,
};

static const struct rbh_value NS_XATTRS_SEQUENCE = {
    .type = RBH_VT_SEQUENCE,
    .sequence = {
        .values = &NS_XATTRS_MAP,
        .count = 1,
    },
};

static const struct rbh_value_pair LOL_PAIR[] = {
    { .key = "ns",           .value = &NS_XATTRS_SEQUENCE },
};

static const struct rbh_value_map LOLILOL = {
    .pairs = LOL_PAIR,
    .count = 1,
};

static void fill_uidgid(struct changelog_rec *record, struct rbh_statx *statx)
{
    struct changelog_ext_uidgid *uidgid;

    statx->stx_mask |= RBH_STATX_UID | RBH_STATX_GID;
    uidgid = changelog_rec_uidgid(record);
    statx->stx_uid = uidgid->cr_uid;
    statx->stx_gid = uidgid->cr_gid;
}

static void fill_open_time(struct changelog_rec *record, struct rbh_statx *statx)
{
    statx->stx_mask |= RBH_STATX_ATIME_SEC | RBH_STATX_ATIME_NSEC;
    statx->stx_atime.tv_sec = record->cr_time >> 30;
    statx->stx_atime.tv_nsec = 0;

    statx->stx_mask |= RBH_STATX_BTIME_SEC | RBH_STATX_BTIME_NSEC;
    statx->stx_btime.tv_sec = record->cr_time >> 30;
    statx->stx_btime.tv_nsec = 0;

    statx->stx_mask |= RBH_STATX_CTIME_SEC | RBH_STATX_CTIME_NSEC;
    statx->stx_ctime.tv_sec = record->cr_time >> 30;
    statx->stx_ctime.tv_nsec = 0;

    statx->stx_mask |= RBH_STATX_MTIME_SEC | RBH_STATX_MTIME_NSEC;
    statx->stx_mtime.tv_sec = record->cr_time >> 30;
    statx->stx_mtime.tv_nsec = 0;
}

static void fill_ns_xattrs(struct changelog_rec *record,
                           struct rbh_value_map *map)
{
    struct rbh_value_pair *submap_pairs;
    struct rbh_id *parent_id = NULL;
    struct rbh_value *parent_value = NULL;
    struct rbh_value_map submap_value;
    struct rbh_value *name_value;
    struct rbh_value_pair *pairs;
    struct rbh_value *map_value;
    struct rbh_value *path_value;
    struct rbh_value *lu_fid_value;

    pairs = malloc(3 * sizeof(*pairs));

    parent_id = rbh_id_from_lu_fid(&record->cr_pfid);
    parent_value = malloc(sizeof(*parent_value));
    pairs[0].key = "parent";
    pairs[0].value = parent_value;
    parent_value->type = RBH_VT_BINARY;
    parent_value->binary.data = parent_id->data;
    parent_value->binary.size = parent_id->size;

    name_value = malloc(sizeof(*name_value));
    pairs[1].key = "name";
    pairs[1].value = name_value;
    name_value->type = RBH_VT_STRING;
    name_value->string = strdup(changelog_rec_name(record));

    map_value = malloc(sizeof(*map_value));
    submap_pairs = malloc(2 * sizeof(*submap_pairs));
    pairs[2].key = "xattrs";
    pairs[2].value = map_value;
    map_value->type = RBH_VT_MAP;
    submap_value.count = 2;
    submap_value.pairs = submap_pairs;
    map_value->map = submap_value;

    path_value = malloc(sizeof(*path_value));
    submap_pairs[0].key = "path";
    submap_pairs[0].value = path_value;
    path_value->type = RBH_VT_STRING;
    path_value->string = strdup(changelog_rec_name(record));

    lu_fid_value = malloc(sizeof(*lu_fid_value));
    submap_pairs[1].key = "fid";
    submap_pairs[1].value = lu_fid_value;
    lu_fid_value->type = RBH_VT_BINARY;
    lu_fid_value->binary.data = (const char *)&record->cr_tfid;
    lu_fid_value->binary.size = sizeof(record->cr_tfid);

    map->pairs = pairs;
    map->count = 3;
}

static const void *
lustre_changelog_iter_next(void *iterator)
{
    struct rbh_statx *rec_statx = calloc(1, sizeof(*rec_statx));
    struct lustre_changelog_iterator *records = iterator;
    struct changelog_rec *record;
    struct rbh_id *id = NULL;

    int rc;

retry:
    rc = llapi_changelog_recv(records->reader, &record);
    if (rc < 0) {
        errno = -rc;
        return NULL;
    }
    if (rc > 0) {
        errno = ENODATA;
        return NULL;
    }

/** check chglog_reader.c:758 */
    switch(record->cr_type) {
    case CL_CREATE:     /* RBH_FET_UPSERT */
        id = rbh_id_from_lu_fid(&record->cr_tfid);
        fill_uidgid(record, rec_statx);
        fill_open_time(record, rec_statx);
        fill_ns_xattrs(record, &NS_XATTRS_MAP.map);
        llapi_changelog_free(&record);
        return rbh_fsevent_upsert_new(id, &LOLILOL, rec_statx, NULL);
    case CL_MKDIR:      /* RBH_FET_UPSERT */
        id = rbh_id_from_lu_fid(&record->cr_tfid);
        llapi_changelog_free(&record);
        return rbh_fsevent_upsert_new(id, NULL, NULL, NULL);
    case CL_HARDLINK:   /* RBH_FET_LINK? */
    case CL_SOFTLINK:   /* RBH_FET_UPSERT + symlink */
    case CL_MKNOD:
    case CL_UNLINK:     /* RBH_FET_UNLINK or RBH_FET_DELETE */
    case CL_RMDIR:      /* RBH_FET_UNLINK or RBH_FET_DELETE */
    case CL_RENAME:     /* RBH_FET_UPSERT */
    case CL_EXT:
    case CL_OPEN:
        break;
    case CL_CLOSE:
        id = rbh_id_from_lu_fid(&record->cr_tfid);
        llapi_changelog_free(&record);
        return rbh_fsevent_upsert_new(id, NULL, NULL, NULL);
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
        return fsevent_from_record(record);
    default: /* CL_MARK */
        goto retry;
    }

    __builtin_unreachable();
}

static void
lustre_changelog_iter_destroy(void *iterator)
{
    struct lustre_changelog_iterator *records = iterator;

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
        error(EXIT_FAILURE, 0, "llapi_changelog_start");

    rc = llapi_changelog_set_xflags(events->reader,
                                    CHANGELOG_EXTRA_FLAG_UIDGID |
                                    CHANGELOG_EXTRA_FLAG_NID |
                                    CHANGELOG_EXTRA_FLAG_OMODE |
                                    CHANGELOG_EXTRA_FLAG_XATTR);
    if (rc < 0)
        error(EXIT_FAILURE, 0, "llapi_changelog_set_xflags");

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
        error(EXIT_FAILURE, 0, "malloc");

    lustre_changelog_init(&source->events, mdtname);

    source->events.values = rbh_sstack_new(sizeof(source->events.values) *
                                           (1 << 7));
    source->source = LUSTRE_SOURCE;
    return &source->source;
}
