/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
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
 * Copyright 2012 Red Hat, Inc.
 */

#ifndef __CONNECTION_HELPERS_H__
#define __CONNECTION_HELPERS_H__

#include "ce-page.h"
#include <nm-remote-settings.h>

typedef struct {
	const char *name;
	GType setting_types[4];
	PageNewConnectionFunc new_connection_func;
	gboolean virtual;
} ConnectionTypeData;

ConnectionTypeData *get_connection_type_list (void);

typedef gboolean (*NewConnectionTypeFilterFunc) (GType type,
                                                 gpointer user_data);
typedef void (*NewConnectionResultFunc) (NMConnection *connection,
                                         gpointer user_data);

void new_connection_dialog      (GtkWindow *parent_window,
                                 NMRemoteSettings *settings,
                                 NewConnectionTypeFilterFunc type_filter_func,
                                 NewConnectionResultFunc result_func,
                                 gpointer user_data);
void new_connection_dialog_full (GtkWindow *parent_window,
                                 NMRemoteSettings *settings,
                                 const char *primary_label,
                                 const char *secondary_label,
                                 NewConnectionTypeFilterFunc type_filter_func,
                                 NewConnectionResultFunc result_func,
                                 gpointer user_data);

void new_connection_of_type (GtkWindow *parent_window,
                             const char *detail,
                             NMRemoteSettings *settings,
                             PageNewConnectionFunc new_func,
                             NewConnectionResultFunc result_func,
                             gpointer user_data);

typedef void (*DeleteConnectionResultFunc) (NMRemoteConnection *connection,
                                            gboolean deleted,
                                            gpointer user_data);

void delete_connection (GtkWindow *parent_window,
                        NMRemoteConnection *connection,
                        DeleteConnectionResultFunc result_func,
                        gpointer user_data);

gboolean connection_supports_ip4 (NMConnection *connection);
gboolean connection_supports_ip6 (NMConnection *connection);

#endif  /* __CONNECTION_HELPERS_H__ */

