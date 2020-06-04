/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifndef RBH_FSEVENTS_READERS_H

#include <stdio.h>

extern const char *mountpoint;

struct rbh_iterator *
fsevents_from_file(FILE *file);

#endif
