/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright © 2018, 2019 Endless Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors:
 *  - Philip Withnall <withnall@endlessm.com>
 */

#pragma once

#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>
#include <libmalcontent/app-filter.h>

G_BEGIN_DECLS

/**
 * MctAppFilterListType:
 * @MCT_APP_FILTER_LIST_BLOCKLIST: Any program in the list is not allowed to
 *    be run.
 * @MCT_APP_FILTER_LIST_ALLOWLIST: Any program not in the list is not allowed
 *    to be run.
 *
 * Different semantics for interpreting an application list.
 *
 * Since: 0.2.0
 */
typedef enum
{
  MCT_APP_FILTER_LIST_BLOCKLIST,
  MCT_APP_FILTER_LIST_ALLOWLIST,
} MctAppFilterListType;

struct _MctAppFilter
{
  gint ref_count;

  uid_t user_id;

  gchar **app_list;  /* (not nullable) (owned) (array zero-terminated=1) */
  MctAppFilterListType app_list_type;

  GVariant *oars_ratings;  /* (type a{ss}) (owned non-floating) */
  gboolean allow_user_installation;
  gboolean allow_system_installation;
};

G_END_DECLS
