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
 * Copyright 2009 - 2017 Red Hat, Inc.
 */

#ifndef __CE_POLKIT_H__
#define __CE_POLKIT_H__

#include <gtk/gtk.h>

#include <NetworkManager.h>

void ce_polkit_connect_widget (GtkWidget *widget,
                               const char *tooltip,
                               const char *auth_tooltip,
                               NMClient *client,
                               NMClientPermission permission);

void ce_polkit_set_widget_validation_error (GtkWidget *widget,
                                            const char *validation_error);

#endif  /* __CE_POLKIT_H__ */
