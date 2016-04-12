/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
 *
 * Rodrigo Moya <rodrigo@gnome-db.org>
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
 * Copyright 2004 - 2014 Red Hat, Inc.
 */

#ifndef NM_CONNECTION_LIST_H
#define NM_CONNECTION_LIST_H

#include <glib-object.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <NetworkManager.h>

#define NM_TYPE_CONNECTION_LIST    (nm_connection_list_get_type ())
#define NM_IS_CONNECTION_LIST(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NM_TYPE_CONNECTION_LIST))
#define NM_CONNECTION_LIST(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), NM_TYPE_CONNECTION_LIST, NMConnectionList))

typedef struct {
	GObject parent;

	/* private data */
	GtkTreeView *connection_list;
	GtkTreeModel *model;
	GtkTreeModelFilter *filter;
	GtkTreeSortable *sortable;
	GType displayed_type;

	NMClient *client;

	GtkBuilder *gui;
	GtkWidget *dialog;

	gboolean signals_connected;
} NMConnectionList;

typedef struct {
	GObjectClass parent_class;

	/* Signals */
	void (*done)  (NMConnectionList *list, gint result);
	void (*editing_done)  (NMConnectionList *list, gint result);
} NMConnectionListClass;

GType             nm_connection_list_get_type (void);
NMConnectionList *nm_connection_list_new (void);

void              nm_connection_list_set_type (NMConnectionList *list, GType ctype);

void              nm_connection_list_present (NMConnectionList *list);
void              nm_connection_list_create (NMConnectionList *list, GType ctype, const char *detail);
void              nm_connection_list_edit (NMConnectionList *list, const gchar *uuid);

#endif
