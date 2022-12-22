/* ply-utils.h - i18n handling
 *
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Written By: Hans de Goede <hdgoede@redhat.com>
 */
#ifndef PLY_I18N_H
#define PLY_I18N_H

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) dgettext(PACKAGE, String)
#else
#define _(String) (String)
#endif

#endif /* PLY_I18N_H */
/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
