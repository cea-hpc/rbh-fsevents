/* SPDX-License-Identifer: LGPL-3.0-or-later */

#include <lustre/lustreapi.h>

#include <robinhood.h>

#include "enricher.h"

int
enrich_lustre_path(const int mount_fd, const struct rbh_id *id, char *path,
                   const size_t usable_size)
{
    const struct lu_fid *fid = rbh_lu_fid_from_id(id);
    long long recno = 0;
    int linkno = 0;

    return llapi_fid2path_at(mount_fd, fid, path, usable_size, &recno, &linkno);
}
