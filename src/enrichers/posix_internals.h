/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef POSIX_INTERNALS_H
#define POSIX_INTERNALS_H

#include <robinhood.h>

struct enricher {
    struct rbh_iterator iterator;

    struct rbh_iterator *fsevents;
    int mount_fd;

    struct rbh_value_pair *pairs;
    size_t pair_count;

    struct rbh_fsevent fsevent;
    struct rbh_statx statx;
    char *symlink;

    int (*callback)(struct enricher *, const struct rbh_fsevent *);
};

int
open_by_id(int mound_fd, const struct rbh_id *id, int flags);

#endif
