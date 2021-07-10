/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef RBH_FSEVENTS_SOURCE_H
#define RBH_FSEVENTS_SOURCE_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <robinhood/iterator.h>

struct source_operations {
    int (*acknowledge)(void *source, size_t index);
};

struct source {
    struct rbh_iterator fsevents;
    const char *name;
    const struct source_operations *ops;
};

struct source *
source_from_file(FILE *file);

static inline int
source_acknowledge(struct source *source, size_t index)
{
    if (source->ops->acknowledge == NULL) {
        errno = ENOTSUP;
        return -1;
    }
    return source->ops->acknowledge(source, index);
}

#endif
