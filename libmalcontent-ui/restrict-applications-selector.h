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
#include <libmalcontent/app-filter.h>


G_BEGIN_DECLS

#define MCT_TYPE_RESTRICT_APPLICATIONS_SELECTOR (mct_restrict_applications_selector_get_type ())
G_DECLARE_FINAL_TYPE (MctRestrictApplicationsSelector, mct_restrict_applications_selector, MCT, RESTRICT_APPLICATIONS_SELECTOR, GtkBox)

MctRestrictApplicationsSelector *mct_restrict_applications_selector_new (MctAppFilter *app_filter);

MctAppFilter *mct_restrict_applications_selector_get_app_filter (MctRestrictApplicationsSelector *self);
void          mct_restrict_applications_selector_set_app_filter (MctRestrictApplicationsSelector *self,
                                                                 MctAppFilter                    *app_filter);

void mct_restrict_applications_selector_build_app_filter (MctRestrictApplicationsSelector *self,
                                                          MctAppFilterBuilder             *builder);

G_END_DECLS
