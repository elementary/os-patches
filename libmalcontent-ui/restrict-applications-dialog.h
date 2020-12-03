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

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <libmalcontent/manager.h>


G_BEGIN_DECLS

#define MCT_TYPE_RESTRICT_APPLICATIONS_DIALOG (mct_restrict_applications_dialog_get_type ())
G_DECLARE_FINAL_TYPE (MctRestrictApplicationsDialog, mct_restrict_applications_dialog, MCT, RESTRICT_APPLICATIONS_DIALOG, GtkDialog)

MctRestrictApplicationsDialog *mct_restrict_applications_dialog_new (MctAppFilter *app_filter,
                                                                     const gchar  *user_display_name);

MctAppFilter *mct_restrict_applications_dialog_get_app_filter (MctRestrictApplicationsDialog *self);
void          mct_restrict_applications_dialog_set_app_filter (MctRestrictApplicationsDialog *self,
                                                               MctAppFilter                  *app_filter);

const gchar *mct_restrict_applications_dialog_get_user_display_name (MctRestrictApplicationsDialog *self);
void         mct_restrict_applications_dialog_set_user_display_name (MctRestrictApplicationsDialog *self,
                                                                     const gchar                   *user_display_name);

void mct_restrict_applications_dialog_build_app_filter (MctRestrictApplicationsDialog *self,
                                                        MctAppFilterBuilder           *builder);

G_END_DECLS
