/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright © 2018, 2019, 2020 Endless Mobile, Inc.
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
 *  - Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *  - Philip Withnall <withnall@endlessm.com>
 */

#pragma once

#include <act/act.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <libmalcontent/malcontent.h>


G_BEGIN_DECLS

#define MCT_TYPE_USER_CONTROLS (mct_user_controls_get_type())
G_DECLARE_FINAL_TYPE (MctUserControls, mct_user_controls, MCT, USER_CONTROLS, GtkGrid)

ActUser *mct_user_controls_get_user (MctUserControls *self);
void     mct_user_controls_set_user (MctUserControls *self,
                                     ActUser         *user);

GPermission *mct_user_controls_get_permission (MctUserControls *self);
void         mct_user_controls_set_permission (MctUserControls *self,
                                               GPermission     *permission);

MctAppFilter *mct_user_controls_get_app_filter (MctUserControls *self);
void          mct_user_controls_set_app_filter (MctUserControls *self,
                                                MctAppFilter    *app_filter);

ActUserAccountType mct_user_controls_get_user_account_type (MctUserControls    *self);
void               mct_user_controls_set_user_account_type (MctUserControls    *self,
                                                            ActUserAccountType  user_account_type);

const gchar *mct_user_controls_get_user_locale (MctUserControls *self);
void         mct_user_controls_set_user_locale (MctUserControls *self,
                                                const gchar     *user_locale);

const gchar *mct_user_controls_get_user_display_name (MctUserControls *self);
void         mct_user_controls_set_user_display_name (MctUserControls *self,
                                                      const gchar     *user_display_name);

void mct_user_controls_build_app_filter (MctUserControls     *self,
                                         MctAppFilterBuilder *builder);

G_END_DECLS
