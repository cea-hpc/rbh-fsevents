/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include <robinhood.h>

static struct rbh_value type = {
    .type = RBH_VT_STRING,
    .string = "type",
};

static struct rbh_value mode = {
    .type = RBH_VT_STRING,
    .string = "mode",
};

static struct rbh_value nlink = {
    .type = RBH_VT_STRING,
    .string = "nlink",
};

static struct rbh_value uid = {
    .type = RBH_VT_STRING,
    .string = "uid",
};

static struct rbh_value gid = {
    .type = RBH_VT_STRING,
    .string = "gid",
};

static struct rbh_value xtime_sec = {
    .type = RBH_VT_STRING,
    .string = "sec",
};

static struct rbh_value xtime_nsec = {
    .type = RBH_VT_STRING,
    .string = "nsec",
};

static struct rbh_value atime_missing_fields[2];

static struct rbh_value atime_mask = {
    .type = RBH_VT_SEQUENCE,
    .sequence = {
        .values = atime_missing_fields,
    },
};

static struct rbh_value_pair _atime = {
    .key = "atime",
    .value = &atime_mask,
};

static struct rbh_value atime = {
    .type = RBH_VT_MAP,
    .map = {
        .pairs = &_atime,
        .count = 1,
    },
};

static struct rbh_value mtime_missing_fields[2];

static struct rbh_value mtime_mask = {
    .type = RBH_VT_SEQUENCE,
    .sequence = {
        .values = mtime_missing_fields,
    },
};

static struct rbh_value_pair _mtime = {
    .key = "mtime",
    .value = &mtime_mask,
};

static struct rbh_value mtime = {
    .type = RBH_VT_MAP,
    .map = {
        .pairs = &_mtime,
        .count = 1,
    },
};

static struct rbh_value ctime_missing_fields[2];

static struct rbh_value ctime_mask = {
    .type = RBH_VT_SEQUENCE,
    .sequence = {
        .values = ctime_missing_fields,
    },
};

static struct rbh_value_pair _ctime = {
    .key = "ctime",
    .value = &ctime_mask,
};

static struct rbh_value ctime_ = {
    .type = RBH_VT_MAP,
    .map = {
        .pairs = &_ctime,
        .count = 1,
    },
};

static struct rbh_value ino = {
    .type = RBH_VT_STRING,
    .string = "ino",
};

static struct rbh_value size = {
    .type = RBH_VT_STRING,
    .string = "size",
};

static struct rbh_value blocks = {
    .type = RBH_VT_STRING,
    .string = "blocks",
};

static struct rbh_value btime_missing_fields[2];

static struct rbh_value btime_mask = {
    .type = RBH_VT_SEQUENCE,
    .sequence = {
        .values = btime_missing_fields,
    },
};

static struct rbh_value_pair _btime = {
    .key = "btime",
    .value = &btime_mask,
};

static struct rbh_value btime = {
    .type = RBH_VT_MAP,
    .map = {
        .pairs = &_btime,
        .count = 1,
    },
};

static struct rbh_value mount_id = {
    .type = RBH_VT_STRING,
    .string = "mount-id",
};

static struct rbh_value blksize = {
    .type = RBH_VT_STRING,
    .string = "blksize",
};

static struct rbh_value attributes = {
    .type = RBH_VT_STRING,
    .string = "attributes",
};

static struct rbh_value xdev_major = {
    .type = RBH_VT_STRING,
    .string = "major",
};

static struct rbh_value xdev_minor = {
    .type = RBH_VT_STRING,
    .string = "minor",
};

static struct rbh_value rdev_missing_fields[2];

static struct rbh_value rdev_mask = {
    .type = RBH_VT_SEQUENCE,
    .sequence = {
        .values = rdev_missing_fields,
    },
};

static struct rbh_value_pair _rdev = {
    .key = "rdev",
    .value = &rdev_mask,
};

static struct rbh_value rdev = {
    .type = RBH_VT_MAP,
    .map = {
        .pairs = &_rdev,
        .count = 1,
    },
};

static struct rbh_value dev_missing_fields[2];

static struct rbh_value dev_mask = {
    .type = RBH_VT_SEQUENCE,
    .sequence = {
        .values = dev_missing_fields,
    },
};

static struct rbh_value_pair _dev = {
    .key = "dev",
    .value = &dev_mask,
};

static struct rbh_value dev = {
    .type = RBH_VT_MAP,
    .map = {
        .pairs = &_dev,
        .count = 1,
    },
};

static struct rbh_value statx_missing_fields[sizeof(RBH_STATX_ALL)];

static struct rbh_value statx_mask = {
    .type = RBH_VT_SEQUENCE,
    .sequence = {
        .values = statx_missing_fields,
        .count = 0,
    },
};

static struct rbh_value_pair partial_fields[2] = {
    {
        .key = "statx",
        .value = &statx_mask,
    }
};

static struct rbh_value partials = {
    .type = RBH_VT_MAP,
    .map = {
        .pairs = partial_fields,
        .count = 1,
    },
};

static struct rbh_value_pair rbh_fsevents = {
    .key = "rbh-fsevents",
    .value = &partials,
};

static struct rbh_fsevent fsevent = {
    .type = RBH_FET_UPSERT,
    .xattrs = {
        .pairs = &rbh_fsevents,
        .count = 1,
    },
};

static void
fsevent_add_missing_statx_fields(uint32_t mask)
{
    statx_mask.sequence.count = 0;

    if (mask & RBH_STATX_TYPE)
        statx_missing_fields[statx_mask.sequence.count++] = type;

    if (mask & RBH_STATX_MODE)
        statx_missing_fields[statx_mask.sequence.count++] = mode;

    if (mask & RBH_STATX_NLINK)
        statx_missing_fields[statx_mask.sequence.count++] = nlink;

    if (mask & RBH_STATX_UID)
        statx_missing_fields[statx_mask.sequence.count++] = uid;

    if (mask & RBH_STATX_GID)
        statx_missing_fields[statx_mask.sequence.count++] = gid;

    if (mask & RBH_STATX_ATIME) {
        statx_missing_fields[statx_mask.sequence.count++] = atime;

        atime_mask.sequence.count = 0;
        if (mask & RBH_STATX_ATIME_SEC)
            atime_missing_fields[atime_mask.sequence.count++] = xtime_sec;
        if (mask & RBH_STATX_ATIME_NSEC)
            atime_missing_fields[atime_mask.sequence.count++] = xtime_nsec;
    }

    if (mask & RBH_STATX_MTIME) {
        statx_missing_fields[statx_mask.sequence.count++] = mtime;

        mtime_mask.sequence.count = 0;
        if (mask & RBH_STATX_MTIME_SEC)
            mtime_missing_fields[mtime_mask.sequence.count++] = xtime_sec;
        if (mask & RBH_STATX_MTIME_NSEC)
            mtime_missing_fields[mtime_mask.sequence.count++] = xtime_nsec;
    }

    if (mask & RBH_STATX_CTIME) {
        statx_missing_fields[statx_mask.sequence.count++] = ctime_;

        ctime_mask.sequence.count = 0;
        if (mask & RBH_STATX_CTIME_SEC)
            ctime_missing_fields[ctime_mask.sequence.count++] = xtime_sec;
        if (mask & RBH_STATX_CTIME_NSEC)
            ctime_missing_fields[ctime_mask.sequence.count++] = xtime_nsec;
    }

    if (mask & RBH_STATX_INO)
        statx_missing_fields[statx_mask.sequence.count++] = ino;

    if (mask & RBH_STATX_SIZE)
        statx_missing_fields[statx_mask.sequence.count++] = size;

    if (mask & RBH_STATX_BLOCKS)
        statx_missing_fields[statx_mask.sequence.count++] = blocks;

    if (mask & RBH_STATX_BTIME) {
        statx_missing_fields[statx_mask.sequence.count++] = btime;

        btime_mask.sequence.count = 0;
        if (mask & RBH_STATX_BTIME_SEC)
            ctime_missing_fields[btime_mask.sequence.count++] = xtime_sec;
        if (mask & RBH_STATX_BTIME_NSEC)
            ctime_missing_fields[btime_mask.sequence.count++] = xtime_nsec;
    }

    if (mask & RBH_STATX_MNT_ID)
        statx_missing_fields[statx_mask.sequence.count++] = mount_id;

    if (mask & RBH_STATX_BLKSIZE)
        statx_missing_fields[statx_mask.sequence.count++] = blksize;

    if (mask & RBH_STATX_ATTRIBUTES)
        statx_missing_fields[statx_mask.sequence.count++] = attributes;

    if (mask & RBH_STATX_RDEV) {
        statx_missing_fields[statx_mask.sequence.count++] = rdev;

        rdev_mask.sequence.count = 0;
        if (mask & RBH_STATX_RDEV_MAJOR)
            ctime_missing_fields[rdev_mask.sequence.count++] = xdev_major;
        if (mask & RBH_STATX_RDEV_MINOR)
            ctime_missing_fields[rdev_mask.sequence.count++] = xdev_minor;
    }

    if (mask & RBH_STATX_DEV) {
        statx_missing_fields[statx_mask.sequence.count++] = dev;

        rdev_mask.sequence.count = 0;
        if (mask & RBH_STATX_DEV_MAJOR)
            ctime_missing_fields[dev_mask.sequence.count++] = xdev_major;
        if (mask & RBH_STATX_DEV_MINOR)
            ctime_missing_fields[dev_mask.sequence.count++] = xdev_minor;
    }
}

struct my_iterator {
    struct rbh_iterator iterator;

    // TODO: add the fields you need here
};

static const void *
fsevent_iter_next(void *iterator)
{
    struct my_iterator *mine = iterator;
    bool is_upsert = false;

    (void)mine;
    // TODO: figure out what the next fsevent should be
    // ...

    // TODO: set the known fields in `fsevent`
    //
    // fsevent.id = ...
    //
    // fsevent.upsert.statx = ...
    // fsevent.upsert.symlink = ...
    //
    // fsevent.link.parent_id = ...
    // fsevent.link.name = ...
    //
    // fsevent.unlink.parent_id = ...
    // fsevent.unlink.name = ...

    if (is_upsert) {
        uint32_t mask = 0; // TODO: figure out which fields should be set
        bool is_new_symlink = 0; // TODO: figure out whether it is true or false

        fsevent_add_missing_statx_fields(mask);

        if (is_new_symlink) {
            partial_fields[1].key = "symlink";
            partials.map.count = 2;
        } else {
            partials.map.count = 1;
        }
    }

    return &fsevent;
}

static void
fsevent_iter_destroy(void *iterator)
{
    (void)iterator;
    return;
}

static const struct rbh_iterator_operations MY_ITER_OPS = {
    .next = fsevent_iter_next,
    .destroy = fsevent_iter_destroy,
};

static const struct rbh_iterator __attribute__((unused)) MY_ITERATOR = {
    .ops = &MY_ITER_OPS,
};
