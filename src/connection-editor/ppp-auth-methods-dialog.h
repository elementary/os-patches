/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
 *
 * Dan Williams <dcbw@redhat.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2008 Red Hat, Inc.
 */

#ifndef PPP_AUTH_METHODS_DIALOG_H
#define PPP_AUTH_METHODS_DIALOG_H

#include <glib.h>
#include <gtk/gtk.h>

GtkWidget *ppp_auth_methods_dialog_new (gboolean refuse_eap,
                                        gboolean refuse_pap,
                                        gboolean refuse_chap,
                                        gboolean refuse_mschap,
                                        gboolean refuse_mschapv2);

void ppp_auth_methods_dialog_get_methods (GtkWidget *dialog,
                                          gboolean *refuse_eap,
                                          gboolean *refuse_pap,
                                          gboolean *refuse_chap,
                                          gboolean *refuse_mschap,
                                          gboolean *refuse_mschapv2);

#endif /* PPP_AUTH_METHODS_DIALOG_H */
