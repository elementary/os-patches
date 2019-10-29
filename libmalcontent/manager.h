/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright © 2019 Endless Mobile, Inc.
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
 * MctGetAppFilterFlags:
 * @MCT_GET_APP_FILTER_FLAGS_NONE: No flags set.
 * @MCT_GET_APP_FILTER_FLAGS_INTERACTIVE: Allow interactive polkit dialogs when
 *    requesting authorization.
 *
 * Flags to control the behaviour of mct_manager_get_app_filter() and
 * mct_manager_get_app_filter_async().
 *
 * Since: 0.3.0
 */
typedef enum
{
  MCT_GET_APP_FILTER_FLAGS_NONE = 0,
  MCT_GET_APP_FILTER_FLAGS_INTERACTIVE,
} MctGetAppFilterFlags;

/**
 * MctSetAppFilterFlags:
 * @MCT_SET_APP_FILTER_FLAGS_NONE: No flags set.
 * @MCT_SET_APP_FILTER_FLAGS_INTERACTIVE: Allow interactive polkit dialogs when
 *    requesting authorization.
 *
 * Flags to control the behaviour of mct_manager_set_app_filter() and
 * mct_manager_set_app_filter_async().
 *
 * Since: 0.3.0
 */
typedef enum
{
  MCT_SET_APP_FILTER_FLAGS_NONE = 0,
  MCT_SET_APP_FILTER_FLAGS_INTERACTIVE,
} MctSetAppFilterFlags;

#define MCT_TYPE_MANAGER mct_manager_get_type ()
G_DECLARE_FINAL_TYPE (MctManager, mct_manager, MCT, MANAGER, GObject)

MctManager   *mct_manager_new (GDBusConnection *connection);

MctAppFilter *mct_manager_get_app_filter        (MctManager            *self,
                                                 uid_t                  user_id,
                                                 MctGetAppFilterFlags   flags,
                                                 GCancellable          *cancellable,
                                                 GError               **error);
void          mct_manager_get_app_filter_async  (MctManager            *self,
                                                 uid_t                  user_id,
                                                 MctGetAppFilterFlags   flags,
                                                 GCancellable          *cancellable,
                                                 GAsyncReadyCallback    callback,
                                                 gpointer               user_data);
MctAppFilter *mct_manager_get_app_filter_finish (MctManager            *self,
                                                 GAsyncResult          *result,
                                                 GError               **error);

gboolean      mct_manager_set_app_filter        (MctManager            *self,
                                                 uid_t                  user_id,
                                                 MctAppFilter          *app_filter,
                                                 MctSetAppFilterFlags   flags,
                                                 GCancellable          *cancellable,
                                                 GError               **error);
void          mct_manager_set_app_filter_async  (MctManager            *self,
                                                 uid_t                  user_id,
                                                 MctAppFilter          *app_filter,
                                                 MctSetAppFilterFlags   flags,
                                                 GCancellable          *cancellable,
                                                 GAsyncReadyCallback    callback,
                                                 gpointer               user_data);
gboolean      mct_manager_set_app_filter_finish (MctManager            *self,
                                                 GAsyncResult          *result,
                                                 GError               **error);

G_END_DECLS
