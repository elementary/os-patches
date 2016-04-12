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
 * Copyright 2007 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright 2007 - 2014 Red Hat, Inc.
 */

#ifndef NM_CONNECTION_EDITOR_H
#define NM_CONNECTION_EDITOR_H

#include <glib-object.h>

#include <NetworkManager.h>

#include "utils.h"

#define NM_TYPE_CONNECTION_EDITOR    (nm_connection_editor_get_type ())
#define NM_IS_CONNECTION_EDITOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NM_TYPE_CONNECTION_EDITOR))
#define NM_CONNECTION_EDITOR(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), NM_TYPE_CONNECTION_EDITOR, NMConnectionEditor))

typedef struct GetSecretsInfo GetSecretsInfo;

typedef struct {
	GObject parent;
	gboolean disposed;

	GtkWindow *parent_window;
	NMClient *client;
	guint permission_id;

	/* private data */
	NMConnection *connection;
	NMConnection *orig_connection;
	gboolean is_new_connection;

	GetSecretsInfo *secrets_call;
	GSList *pending_secrets_calls;

	GtkWidget *all_checkbutton;
	NMClientPermissionResult can_modify;

	GSList *initializing_pages;
	GSList *pages;
	GtkBuilder *builder;
	GtkWidget *window;
	GtkWidget *ok_button;
	GtkWidget *cancel_button;
	GtkWidget *export_button;

	gboolean busy;
	gboolean init_run;
	guint validate_id;

	char *last_validation_error;

	GHashTable *inter_page_hash;
} NMConnectionEditor;

typedef struct {
	GObjectClass parent_class;

	/* Signals */
	void (*done)  (NMConnectionEditor *editor, gint result, GError *error);
} NMConnectionEditorClass;

typedef enum {
	/* Add item for inter-page changes here */
	INTER_PAGE_CHANGE_WIFI_MODE = 1,
} InterPageChangeType;

GType               nm_connection_editor_get_type (void);
NMConnectionEditor *nm_connection_editor_new (GtkWindow *parent_window,
                                              NMConnection *connection,
                                              NMClient *client);
NMConnectionEditor *nm_connection_editor_get (NMConnection *connection);
NMConnectionEditor *nm_connection_editor_get_master (NMConnection *slave);

void                nm_connection_editor_present (NMConnectionEditor *editor);
void                nm_connection_editor_run (NMConnectionEditor *editor);
NMConnection *      nm_connection_editor_get_connection (NMConnectionEditor *editor);
GtkWindow *         nm_connection_editor_get_window (NMConnectionEditor *editor);
gboolean            nm_connection_editor_get_busy (NMConnectionEditor *editor);
void                nm_connection_editor_set_busy (NMConnectionEditor *editor, gboolean busy);

void                nm_connection_editor_error (GtkWindow *parent,
                                                const char *heading,
                                                const char *format,
                                                ...);
void                nm_connection_editor_warning (GtkWindow *parent,
                                                  const char *heading,
                                                  const char *format,
                                                  ...);

void               nm_connection_editor_inter_page_set_value (NMConnectionEditor *editor,
                                                              InterPageChangeType type,
                                                              gpointer value);
gboolean           nm_connection_editor_inter_page_get_value (NMConnectionEditor *editor,
                                                              InterPageChangeType type,
                                                              gpointer *value);
void               nm_connection_editor_inter_page_clear_data (NMConnectionEditor *editor);

#endif
