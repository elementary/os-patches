/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2010 Nokia Corporation.
 *
 * Contact: Alberto Mardegan <alberto.mardegan@canonical.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include "ag-debug.h"

static const GDebugKey debug_keys[] = {
    { "time", AG_DEBUG_TIME },
    { "refs", AG_DEBUG_REFS },
    { "locks", AG_DEBUG_LOCKS },
    { "queries", AG_DEBUG_QUERIES },
    { "info", AG_DEBUG_INFO },
};

static AgDebugLevel debug_level = AG_DEBUG_LOCKS;

void
_ag_debug_init (void)
{
    const gchar *env;
    static gboolean initialized = FALSE;

    if (initialized) return;

    initialized = TRUE;
    env = g_getenv ("AG_DEBUG");
    if (env)
    {
        debug_level = g_parse_debug_string (env, debug_keys,
                                            G_N_ELEMENTS(debug_keys));
    }
}

AgDebugLevel
_ag_debug_get_level (void)
{
    return debug_level;
}

