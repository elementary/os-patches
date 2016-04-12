/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright 2015 Red Hat, Inc.
 */


/* WARNING: this file is private API between nm-applet and various GNOME
 * bits; it may change without notice and is not guaranteed to be stable.
 */

#ifndef NMA_UI_UTILS_H
#define NMA_UI_UTILS_H

#include <glib.h>
#include <gtk/gtk.h>
#include <NetworkManager.h>

void nma_utils_setup_password_storage (GtkWidget *passwd_entry,
                                       NMSettingSecretFlags initial_flags,
                                       NMSetting *setting,
                                       const char *password_flags_name,
                                       gboolean with_not_required,
                                       gboolean sensitive_ask);
NMSettingSecretFlags nma_utils_menu_to_secret_flags (GtkWidget *passwd_entry);
void nma_utils_update_password_storage (GtkWidget *passwd_entry,
                                        NMSettingSecretFlags secret_flags,
                                        NMSetting *setting,
                                        const char *password_flags_name);

#endif /* NMA_UI_UTILS_H */

