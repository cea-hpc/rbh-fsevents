/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>

#include "readers.h"

struct rbh_iterator *
fsevents_from_file(FILE *file)
{
    if (fclose(file) == EOF)
        error(EXIT_FAILURE, errno, "fclose");

    /* TODO: parse yaml documents and enrich them if needed/possible */

    error(EXIT_FAILURE, ENOSYS, "%s", __func__);
    __builtin_unreachable();
}
