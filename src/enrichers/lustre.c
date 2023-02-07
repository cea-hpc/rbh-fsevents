/* SPDX-License-Identifier: LGPL-3.0-or-later */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <robinhood.h>
#include <robinhood/backends/lustre_attrs.h>

#include "enricher.h"
#include "internals.h"

#define MIN_XATTR_VALUES_ALLOC 32
static __thread struct rbh_sstack *xattrs_values;

static void __attribute__((destructor))
exit_xattrs_values(void)
{
    if (xattrs_values)
        rbh_sstack_destroy(xattrs_values);
}

/* TODO? Make this call available in librobinhood API */
static const struct rbh_value *
_get_value_from_map(const struct rbh_value_map *map, const char *key)
{
    for (size_t i = 0; i < map->count; ++i) {
        if(strcmp(map->pairs[i].key, key) == 0)
            return map->pairs[i].value;
    }

    return NULL;
}


static int
lustre_callback(struct enricher *enricher, const struct rbh_fsevent *original)
{
    static const int STATX_FLAGS = AT_STATX_FORCE_SYNC | AT_EMPTY_PATH
                                 | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW;
    struct rbh_value_pair *pairs = enricher->pairs;
    const struct rbh_value *fsevents;
    const struct rbh_value *xattrs;
    const struct rbh_value *inner_value;
    const struct rbh_value *end;
    struct rbh_statx statxbuf;
    int save_errno;
    int size;
    int fd;
    int rc;

    fsevents = _get_value_from_map(&original->xattrs, "rbh-fsevents");
    if (fsevents == NULL)
        return 0;
    if (fsevents->type != RBH_VT_MAP) {
        errno = EINVAL;
        return -1;
    }

    xattrs = _get_value_from_map(&fsevents->map, "xattrs");
    if (xattrs == NULL)
        return 0;
    if (xattrs->type != RBH_VT_SEQUENCE) {
        errno = EINVAL;
        return -1;
    }

    inner_value = &xattrs->sequence.values[0];
    end = xattrs->sequence.values + xattrs->sequence.count;
    while (inner_value != end &&
           inner_value->type != RBH_VT_STRING &&
           strcmp(inner_value->string, "lustre"))
        ++inner_value;

    if (inner_value == end)
        return 0;

    if (xattrs_values == NULL) {
        xattrs_values = rbh_sstack_new(MIN_XATTR_VALUES_ALLOC *
            sizeof(struct rbh_value *));
        if (xattrs_values == NULL)
            return -1;
    }

    fd = open_by_id(enricher->mount_fd, &original->id,
                    O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0 && errno == ELOOP)
        /* If the file to open is a symlink, reopen it with O_PATH set */
        fd = open_by_id(enricher->mount_fd, &original->id,
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

    size = lustre_get_attrs(fd, statxbuf.stx_mode,
                            &pairs[enricher->fsevent.xattrs.count],
                            xattrs_values);
    enricher->fsevent.xattrs.count += size;

    save_errno = errno;
    close(fd);
    errno = save_errno;

    return size;
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
