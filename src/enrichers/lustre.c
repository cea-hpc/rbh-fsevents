/* SPDX-License-Identifer: LGPL-3.0-or-later */

#include <lustre/lustreapi.h>
#include <string.h>

#include <robinhood.h>

#include "enricher.h"

int
enrich_lustre_path(const int mount_fd, const struct rbh_id *id, char *path,
                   const size_t usable_size)
{
    const struct lu_fid *fid = rbh_lu_fid_from_id(id);
    char tmp_path[usable_size];
    long long recno = 0;
    int linkno = 0;
    int rc;

    rc = llapi_fid2path_at(mount_fd, fid, tmp_path, usable_size, &recno,
                           &linkno);
    if (rc)
        return rc;

    path[0] = '/';

    if (tmp_path[0] == '/' && tmp_path[1] == 0)
        path[1] = 0;
    else
        strncpy(&path[1], tmp_path, usable_size - 1);

    return 0;
}
