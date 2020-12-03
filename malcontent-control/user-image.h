/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright © 2015 Red Hat, Inc.
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
 *  - Ondrej Holy <oholy@redhat.com>
 */

#pragma once

#include <gtk/gtk.h>
#include <act/act.h>


G_BEGIN_DECLS

#define MCT_TYPE_USER_IMAGE (mct_user_image_get_type ())
G_DECLARE_FINAL_TYPE (MctUserImage, mct_user_image, MCT, USER_IMAGE, GtkImage)

GtkWidget *mct_user_image_new      (void);
void       mct_user_image_set_user (MctUserImage *image,
                                    ActUser      *user);

G_END_DECLS
