/* SPDX-License-Identifier: LGPL-3.0-or-later */

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <lustre/lustreapi.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <robinhood.h>
#include <robinhood/backends/lustre_attrs.h>

#include "enricher.h"
#include "internals.h"

#define MIN_XATTR_VALUES_ALLOC (1 << 14)
static __thread struct rbh_sstack *xattrs_values;

static void __attribute__((destructor))
exit_xattrs_values(void)
{
    if (xattrs_values)
        rbh_sstack_destroy(xattrs_values);
}

/* TODO? Make this call available in librobinhood API */
static int
_fetch_value_from_map(const struct rbh_value_map *map, const char *key,
                    const struct rbh_value **value)
{
    for (size_t i = 0; i < map->count; ++i) {
        if(strcmp(map->pairs[i].key, key) == 0) {
            if (value)
                *value = map->pairs[i].value;
            return 0;
        }
    }

    return -1;
}

static int
enrich_path(const int mount_fd, const struct rbh_id *id, const char *name,
            struct rbh_sstack *xattrs_values, struct rbh_value **_value)
{
    const struct lu_fid *fid = rbh_lu_fid_from_id(id);
    size_t name_length = strlen(name);
    long long recno = 0;
    size_t path_length;
    int linkno = 0;
    char *path;
    int rc;

    path = rbh_sstack_push(xattrs_values, NULL, PATH_MAX);
    if (path == NULL)
        return -1;

    /* lustre path */
    path[0] = '/';
    rc = llapi_fid2path_at(mount_fd, fid, path + 1, PATH_MAX - name_length - 2,
                           &recno, &linkno);
    if (rc)
        return rc;

    if (path[1] == '/' && path[2] == 0)
        path[0] = 0;

    path_length = strlen(path);
    /* remove the extra space left in the sstack */
    rc = rbh_sstack_pop(xattrs_values,
                        PATH_MAX - (path_length + name_length + 2));
    if (rc)
        return -1;

    path[path_length] = '/';
    strcpy(&path[path_length + 1], name);

    *_value = rbh_sstack_push(xattrs_values, NULL, sizeof(**_value));
    if (*_value == NULL)
        return -1;

    (*_value)->type = RBH_VT_STRING;
    (*_value)->string = path;

    return 1;
}

static int
enrich_lustre(int mount_fd, const struct rbh_id *id,
              struct rbh_sstack *xattrs_values, struct rbh_value_pair *pair)
{
    static const int STATX_FLAGS = AT_STATX_FORCE_SYNC | AT_EMPTY_PATH
                                 | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW;
    struct rbh_statx statxbuf;
    int save_errno;
    int size;
    int fd;
    int rc;

    fd = open_by_id(mount_fd, id, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0 && errno == ELOOP)
        /* If the file to open is a symlink, reopen it with O_PATH set */
        fd = open_by_id(mount_fd, id,
                        O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_PATH);
    if (fd < 0)
        return -1;

    rc = rbh_statx(fd, "", STATX_FLAGS, RBH_STATX_MODE, &statxbuf);
    if (rc == -1) {
        save_errno = errno;
        close(fd);
        errno = save_errno;
        return -1;
    }

    size = lustre_get_attrs(fd, statxbuf.stx_mode, pair, xattrs_values);

    save_errno = errno;
    close(fd);
    errno = save_errno;

    return size;
}

static int
lustre_callback(struct enricher *enricher, const struct rbh_fsevent *original)
{
    struct rbh_value_pair *pairs = enricher->pairs;
    const struct rbh_value *fsevents;
    int size;
    int rc;

    rc = _fetch_value_from_map(&original->xattrs, "rbh-fsevents", &fsevents);
    if (rc || fsevents == NULL)
        return 0;
    if (fsevents->type != RBH_VT_MAP) {
        errno = EINVAL;
        return -1;
    }

    if (xattrs_values == NULL) {
        xattrs_values = rbh_sstack_new(MIN_XATTR_VALUES_ALLOC *
                                           sizeof(struct rbh_value *));
        if (xattrs_values == NULL)
            return -1;
    }

    rc = _fetch_value_from_map(&fsevents->map, "path", NULL);
    if (!rc) {
        struct rbh_value *value;

        size = enrich_path(enricher->mount_fd, original->link.parent_id,
                           original->link.name, xattrs_values, &value);
        if (size == -1)
            return -1;

        pairs[enricher->fsevent.xattrs.count].key = "path";
        pairs[enricher->fsevent.xattrs.count].value = value;
        enricher->fsevent.xattrs.count += size;

        return size;
    }

    rc = _fetch_value_from_map(&fsevents->map, "lustre", NULL);
    if (!rc) {
        size = enrich_lustre(enricher->mount_fd, &original->id,
                             xattrs_values,
                             &pairs[enricher->fsevent.xattrs.count]);
        if (size == -1)
            return -1;

        enricher->fsevent.xattrs.count += size;

        return size;
    }

    return 0;
}

struct rbh_iterator *
lustre_iter_enrich(struct rbh_iterator *fsevents, int mount_fd)
{
    struct rbh_iterator *iter = posix_iter_enrich(fsevents, mount_fd);
    struct enricher *enricher = (struct enricher *)iter;

    if (iter == NULL)
        return NULL;

    enricher->callback = lustre_callback;

    return iter;
}
