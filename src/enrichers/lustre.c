/* SPDX-License-Identifer: LGPL-3.0-or-later */

#include <linux/limits.h>

#include <lustre/lustreapi.h>

#include <robinhood.h>

#include "enricher.h"

/* To retrieve the path of the file, we cannot use the rbh_id of the file
 * because we have no way of differentiating between hardlinks, since they all
 * refer to the same rbh_id. Therefore, we will instead use the rbh_id of the
 * parent directory, and append the name of the file to it.
 */
int
enrich_lustre_path(const struct rbh_id *id, const int mount_fd,
                   const char *name, struct rbh_sstack *xattrs_values,
                   struct rbh_value **_value)
{
    const struct lu_fid *fid = rbh_lu_fid_from_id(id);
    struct rbh_value *value;
    long long recno;
    int linkno = 0;
    char *path;
    int rc;

    path = rbh_sstack_push(xattrs_values, NULL, PATH_MAX);
    if (path == NULL)
        return -1;

    /* Only retrieve the stack size allocated minus the length of the name to
     * preprare the append.
     */
    rc = llapi_fid2path_at(mount_fd, fid, path, PATH_MAX - 1 - strlen(name),
                           &recno, &linkno);
    if (rc)
        return -1;

    /* remove the extra space left in the sstack */
    rc = rbh_sstack_pop(xattrs_values, PATH_MAX - (strlen(path) + 1));
    if (rc)
        return -1;

    strcat(path, name);

    value = rbh_sstack_push(xattrs_values, NULL, sizeof(*value));
    if (value == NULL)
        return -1;

    value->type = RBH_VT_STRING;
    value->string = path;

    *_value = value;

    return 0;
}
