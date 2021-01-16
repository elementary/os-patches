/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright © 2020 Endless Mobile, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  - Philip Withnall <withnall@endlessm.com>
 */

#pragma once

#include <act/act.h>
#include <gtk/gtk.h>


G_BEGIN_DECLS

#define MCT_TYPE_USER_SELECTOR (mct_user_selector_get_type ())
G_DECLARE_FINAL_TYPE (MctUserSelector, mct_user_selector, MCT, USER_SELECTOR, GtkBox)

MctUserSelector *mct_user_selector_new (ActUserManager *user_manager);

ActUser *mct_user_selector_get_user (MctUserSelector *self);

gboolean mct_user_selector_select_user_by_username (MctUserSelector *self,
                                                    const gchar     *username);

G_END_DECLS
