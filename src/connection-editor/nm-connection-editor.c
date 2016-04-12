/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
 *
 * Rodrigo Moya <rodrigo@gnome-db.org>
 * Dan Williams <dcbw@redhat.com>
 * Tambet Ingo <tambet@gmail.com>
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
 * Copyright 2007 - 2014 Red Hat, Inc.
 * Copyright 2007 - 2008 Novell, Inc.
 */

#include "nm-default.h"

#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <gdk/gdkx.h>

#include "nm-connection-editor.h"

#include "ce-page.h"
#include "page-general.h"
#include "page-ethernet.h"
#include "page-8021x-security.h"
#include "page-wifi.h"
#include "page-wifi-security.h"
#include "page-ip4.h"
#include "page-ip6.h"
#include "page-dsl.h"
#include "page-mobile.h"
#include "page-bluetooth.h"
#include "page-ppp.h"
#include "page-vpn.h"
#include "page-infiniband.h"
#include "page-bond.h"
#include "page-team.h"
#include "page-team-port.h"
#include "page-bridge.h"
#include "page-bridge-port.h"
#include "page-vlan.h"
#include "page-dcb.h"
#include "ce-polkit-button.h"
#include "vpn-helpers.h"
#include "eap-method.h"

extern gboolean nm_ce_keep_above;

G_DEFINE_TYPE (NMConnectionEditor, nm_connection_editor, G_TYPE_OBJECT)

enum {
	EDITOR_DONE,
	EDITOR_LAST_SIGNAL
};

static guint editor_signals[EDITOR_LAST_SIGNAL] = { 0 };

static GHashTable *active_editors;

static gboolean nm_connection_editor_set_connection (NMConnectionEditor *editor,
                                                     NMConnection *connection,
                                                     GError **error);

struct GetSecretsInfo {
	NMConnectionEditor *self;
	CEPage *page;
	char *setting_name;
	gboolean canceled;
};

#define SECRETS_TAG "secrets-setting-name"
#define ORDER_TAG "page-order"

static void
nm_connection_editor_update_title (NMConnectionEditor *editor)
{
	NMSettingConnection *s_con;
	const char *id;

	g_return_if_fail (editor != NULL);

	s_con = nm_connection_get_setting_connection (editor->connection);
	g_assert (s_con);

	id = nm_setting_connection_get_id (s_con);
	if (id && strlen (id)) {
		char *title = g_strdup_printf (_("Editing %s"), id);
		gtk_window_set_title (GTK_WINDOW (editor->window), title);
		g_free (title);
	} else
		gtk_window_set_title (GTK_WINDOW (editor->window), _("Editing un-named connection"));
}

static gboolean
ui_to_setting (NMConnectionEditor *editor, GError **error)
{
	NMSettingConnection *s_con;
	GtkWidget *widget;
	const char *name;

	s_con = nm_connection_get_setting_connection (editor->connection);
	g_assert (s_con);

	widget = GTK_WIDGET (gtk_builder_get_object (editor->builder, "connection_name"));
	name = gtk_entry_get_text (GTK_ENTRY (widget));

	g_object_set (G_OBJECT (s_con), NM_SETTING_CONNECTION_ID, name, NULL);
	nm_connection_editor_update_title (editor);

	if (!name || !strlen (name)) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Missing connection name"));
		return FALSE;
	}

	return TRUE;
}

static gboolean
editor_is_initialized (NMConnectionEditor *editor)
{
	return (g_slist_length (editor->initializing_pages) == 0);
}

static void
update_sensitivity (NMConnectionEditor *editor)
{
	NMSettingConnection *s_con;
	gboolean actionable = FALSE, authorized = FALSE, sensitive = FALSE;
	GtkWidget *widget;
	GSList *iter;

	s_con = nm_connection_get_setting_connection (editor->connection);

	/* Can't modify read-only connections; can't modify anything before the
	 * editor is initialized either.
	 */
	if (   !nm_setting_connection_get_read_only (s_con)
	    && editor_is_initialized (editor)) {
		if (editor->can_modify) {
			actionable = ce_polkit_button_get_actionable (CE_POLKIT_BUTTON (editor->ok_button));
			authorized = ce_polkit_button_get_authorized (CE_POLKIT_BUTTON (editor->ok_button));
		}

		/* If the user cannot ever be authorized to change system connections,
		 * we desensitize the entire dialog.
		 */
		sensitive = authorized;
	}

	/* Cancel button is always sensitive */
	gtk_widget_set_sensitive (GTK_WIDGET (editor->cancel_button), TRUE);

	widget = GTK_WIDGET (gtk_builder_get_object (editor->builder, "connection_name_label"));
	gtk_widget_set_sensitive (widget, sensitive);

	widget = GTK_WIDGET (gtk_builder_get_object (editor->builder, "connection_name"));
	gtk_widget_set_sensitive (widget, sensitive);

	for (iter = editor->pages; iter; iter = g_slist_next (iter)) {
		widget = ce_page_get_page (CE_PAGE (iter->data));
		gtk_widget_set_sensitive (widget, sensitive);
	}
}

static void
connection_editor_validate (NMConnectionEditor *editor)
{
	NMSettingConnection *s_con;
	GSList *iter;
	char *validation_error = NULL;
	GError *error = NULL;

	if (!editor_is_initialized (editor)) {
		validation_error = g_strdup (_("Editor initializing..."));
		goto done;
	}

	s_con = nm_connection_get_setting_connection (editor->connection);
	g_assert (s_con);
	if (nm_setting_connection_get_read_only (s_con)) {
		validation_error = g_strdup (_("Connection cannot be modified"));
		goto done;
	}

	if (!ui_to_setting (editor, &error)) {
		validation_error = g_strdup (error->message);
		g_clear_error (&error);
		goto done;
	}

	for (iter = editor->pages; iter; iter = g_slist_next (iter)) {
		if (!ce_page_validate (CE_PAGE (iter->data), editor->connection, &error)) {
			if (!validation_error) {
				validation_error = g_strdup_printf (_("Invalid setting %s: %s"),
				                                    CE_PAGE (iter->data)->title,
				                                    error->message);
			}
			g_clear_error (&error);
		}
	}

done:
	if (g_strcmp0 (validation_error, editor->last_validation_error) != 0) {
		if (editor->last_validation_error && !validation_error)
			g_message ("Connection validates and can be saved");
		else if (validation_error)
			g_message ("Cannot save connection due to error: %s", validation_error);
		g_free (editor->last_validation_error);
		editor->last_validation_error = g_strdup (validation_error);
	}
	ce_polkit_button_set_validation_error (CE_POLKIT_BUTTON (editor->ok_button), validation_error);
	gtk_widget_set_sensitive (editor->export_button, !!validation_error);

	update_sensitivity (editor);

	g_free (validation_error);
}

static void
ok_button_actionable_cb (GtkWidget *button,
                         gboolean actionable,
                         NMConnectionEditor *editor)
{
	connection_editor_validate (editor);
}

static void
permissions_changed_cb (NMClient *client,
                        NMClientPermission permission,
                        NMClientPermissionResult result,
                        NMConnectionEditor *editor)
{
	if (permission != NM_CLIENT_PERMISSION_SETTINGS_MODIFY_SYSTEM)
		return;

	if (result == NM_CLIENT_PERMISSION_RESULT_YES || result == NM_CLIENT_PERMISSION_RESULT_AUTH)
		editor->can_modify = TRUE;
	else
		editor->can_modify = FALSE;

	connection_editor_validate (editor);
}

static void
destroy_inter_page_item (gpointer data)
{
	return;
}

static void
nm_connection_editor_init (NMConnectionEditor *editor)
{
	GtkWidget *dialog;
	GError *error = NULL;
	const char *objects[] = { "nm-connection-editor", NULL };

	editor->builder = gtk_builder_new ();

	if (!gtk_builder_add_objects_from_file (editor->builder,
	                                        UIDIR "/nm-connection-editor.ui",
	                                        (char **) objects,
	                                        &error)) {
		g_warning ("Couldn't load builder file " UIDIR "/nm-connection-editor.ui: %s", error->message);
		g_error_free (error);

		dialog = gtk_message_dialog_new (NULL, 0,
		                                 GTK_MESSAGE_ERROR,
		                                 GTK_BUTTONS_OK,
		                                 "%s",
		                                 _("The connection editor could not find some required resources (the .ui file was not found)."));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		gtk_main_quit ();
		return;
	}

	editor->window = GTK_WIDGET (gtk_builder_get_object (editor->builder, "nm-connection-editor"));
	if (nm_ce_keep_above)
		gtk_window_set_keep_above (GTK_WINDOW (editor->window), TRUE);

	editor->cancel_button = GTK_WIDGET (gtk_builder_get_object (editor->builder, "cancel_button"));
	editor->export_button = GTK_WIDGET (gtk_builder_get_object (editor->builder, "export_button"));

	editor->inter_page_hash = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) destroy_inter_page_item);
}

static void
get_secrets_info_free (GetSecretsInfo *info)
{
	g_free (info->setting_name);
	g_free (info);
}

static void
dispose (GObject *object)
{
	NMConnectionEditor *editor = NM_CONNECTION_EDITOR (object);
	GSList *iter;

	if (editor->disposed)
		goto out;
	editor->disposed = TRUE;

	if (active_editors)
		g_hash_table_remove (active_editors, editor->orig_connection);

	g_slist_foreach (editor->initializing_pages, (GFunc) g_object_unref, NULL);
	g_slist_free (editor->initializing_pages);
	editor->initializing_pages = NULL;

	g_slist_foreach (editor->pages, (GFunc) g_object_unref, NULL);
	g_slist_free (editor->pages);
	editor->pages = NULL;

	/* Mark any in-progress secrets call as canceled; it will clean up after itself. */
	if (editor->secrets_call)
		editor->secrets_call->canceled = TRUE;

	/* Kill any pending secrets calls */
	for (iter = editor->pending_secrets_calls; iter; iter = g_slist_next (iter)) {
		get_secrets_info_free ((GetSecretsInfo *) iter->data);
	}
	g_slist_free (editor->pending_secrets_calls);
	editor->pending_secrets_calls = NULL;

	if (editor->validate_id) {
		g_source_remove (editor->validate_id);
		editor->validate_id = 0;
	}

	if (editor->connection) {
		g_object_unref (editor->connection);
		editor->connection = NULL;
	}
	if (editor->orig_connection) {
		g_object_unref (editor->orig_connection);
		editor->orig_connection = NULL;
	}
	if (editor->window) {
		gtk_widget_destroy (editor->window);
		editor->window = NULL;
	}
	if (editor->parent_window) {
		g_object_unref (editor->parent_window);
		editor->parent_window = NULL;
	}
	if (editor->builder) {
		g_object_unref (editor->builder);
		editor->builder = NULL;
	}

	g_signal_handler_disconnect (editor->client, editor->permission_id);
	g_object_unref (editor->client);

	g_clear_pointer (&editor->last_validation_error, g_free);

	g_hash_table_destroy (editor->inter_page_hash);

out:
	G_OBJECT_CLASS (nm_connection_editor_parent_class)->dispose (object);
}

static void
nm_connection_editor_class_init (NMConnectionEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* virtual methods */
	object_class->dispose = dispose;

	/* Signals */
	editor_signals[EDITOR_DONE] =
		g_signal_new ("done",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_FIRST,
		              G_STRUCT_OFFSET (NMConnectionEditorClass, done),
		              NULL, NULL, NULL,
		              G_TYPE_NONE, 1, GTK_TYPE_RESPONSE_TYPE);
}

NMConnectionEditor *
nm_connection_editor_new (GtkWindow *parent_window,
                          NMConnection *connection,
                          NMClient *client)
{
	NMConnectionEditor *editor;
	GtkWidget *hbox;
	gboolean is_new;
	GError *error = NULL;

	g_return_val_if_fail (NM_IS_CONNECTION (connection), NULL);

	is_new = !nm_client_get_connection_by_uuid (client, nm_connection_get_uuid (connection));

	editor = g_object_new (NM_TYPE_CONNECTION_EDITOR, NULL);
	editor->parent_window = parent_window ? g_object_ref (parent_window) : NULL;
	editor->client = g_object_ref (client);
	editor->is_new_connection = is_new;

	editor->can_modify = nm_client_get_permission_result (client, NM_CLIENT_PERMISSION_SETTINGS_MODIFY_SYSTEM);
	editor->permission_id = g_signal_connect (editor->client,
	                                          "permission-changed",
	                                          G_CALLBACK (permissions_changed_cb),
	                                          editor);

	editor->ok_button = ce_polkit_button_new (_("_Save"),
	                                          _("Save any changes made to this connection."),
	                                          _("Authenticate to save this connection for all users of this machine."),
	                                          GTK_STOCK_APPLY,
	                                          client,
	                                          NM_CLIENT_PERMISSION_SETTINGS_MODIFY_SYSTEM);
	gtk_button_set_use_underline (GTK_BUTTON (editor->ok_button), TRUE);

	g_signal_connect (editor->ok_button, "actionable",
	                  G_CALLBACK (ok_button_actionable_cb), editor);
	g_signal_connect (editor->ok_button, "authorized",
	                  G_CALLBACK (ok_button_actionable_cb), editor);
	hbox = GTK_WIDGET (gtk_builder_get_object (editor->builder, "action_area_hbox"));
	gtk_box_pack_end (GTK_BOX (hbox), editor->ok_button, TRUE, TRUE, 0);
	gtk_widget_show_all (editor->ok_button);

	if (!nm_connection_editor_set_connection (editor, connection, &error)) {
		nm_connection_editor_error (parent_window,
		                            is_new ? _("Could not create connection") : _("Could not edit connection"),
		                            "%s",
		                            error ? error->message : _("Unknown error creating connection editor dialog."));
		g_clear_error (&error);
		g_object_unref (editor);
		return NULL;
	}

	if (!active_editors)
		active_editors = g_hash_table_new (NULL, NULL);
	g_hash_table_insert (active_editors, connection, editor);

	return editor;
}

NMConnectionEditor *
nm_connection_editor_get (NMConnection *connection)
{
	if (!active_editors)
		return NULL;

	return g_hash_table_lookup (active_editors, connection);
}

/* Returns an editor for @slave's master, if any */
NMConnectionEditor *
nm_connection_editor_get_master (NMConnection *slave)
{
	GHashTableIter iter;
	gpointer connection, editor;
	NMSettingConnection *s_con;
	const char *master;

	if (!active_editors)
		return NULL;

	s_con = nm_connection_get_setting_connection (slave);
	master = nm_setting_connection_get_master (s_con);
	if (!master)
		return NULL;

	g_hash_table_iter_init (&iter, active_editors);
	while (g_hash_table_iter_next (&iter, &connection, &editor)) {
		if (!g_strcmp0 (master, nm_connection_get_uuid (connection)))
			return editor;
		if (!g_strcmp0 (master, nm_connection_get_interface_name (connection)))
			return editor;
	}

	return NULL;
}

NMConnection *
nm_connection_editor_get_connection (NMConnectionEditor *editor)
{
	g_return_val_if_fail (NM_IS_CONNECTION_EDITOR (editor), NULL);

	return editor->orig_connection;
}

static void
populate_connection_ui (NMConnectionEditor *editor)
{
	NMSettingConnection *s_con;
	GtkWidget *name;

	name = GTK_WIDGET (gtk_builder_get_object (editor->builder, "connection_name"));

	s_con = nm_connection_get_setting_connection (editor->connection);
	gtk_entry_set_text (GTK_ENTRY (name), s_con ? nm_setting_connection_get_id (s_con) : NULL);
	gtk_widget_set_tooltip_text (name, nm_connection_get_uuid (editor->connection));

	g_signal_connect_swapped (name, "changed", G_CALLBACK (connection_editor_validate), editor);

	connection_editor_validate (editor);
}

static void
page_changed (CEPage *page, gpointer user_data)
{
	NMConnectionEditor *editor = NM_CONNECTION_EDITOR (user_data);
	GSList *iter;

	/* Do page interdependent changes */
	for (iter = editor->pages; iter; iter = g_slist_next (iter))
		ce_page_inter_page_change (CE_PAGE (iter->data));

	if (editor_is_initialized (editor))
		nm_connection_editor_inter_page_clear_data (editor);

	connection_editor_validate (editor);
}

static gboolean
idle_validate (gpointer user_data)
{
	NMConnectionEditor *editor = NM_CONNECTION_EDITOR (user_data);

	editor->validate_id = 0;
	connection_editor_validate (editor);
	return FALSE;
}

static void
recheck_initialization (NMConnectionEditor *editor)
{
	GtkNotebook *notebook;

	if (!editor_is_initialized (editor) || editor->init_run)
		return;

	editor->init_run = TRUE;

	populate_connection_ui (editor);

	/* Show the second page (the connection-type-specific data) first */
	notebook = GTK_NOTEBOOK (gtk_builder_get_object (editor->builder, "notebook"));
	gtk_notebook_set_current_page (notebook, 1);

	/* When everything is initialized, re-present the window to ensure it's on top */
	nm_connection_editor_present (editor);

	/* Validate the connection from an idle handler to ensure that stuff like
	 * GtkFileChoosers have had a chance to asynchronously find their files.
	 */
	if (editor->validate_id)
		g_source_remove (editor->validate_id);
	editor->validate_id = g_idle_add (idle_validate, editor);
}

static void
page_initialized (CEPage *page, GError *error, gpointer user_data)
{
	NMConnectionEditor *editor = NM_CONNECTION_EDITOR (user_data);
	GtkWidget *widget, *parent;
	GtkNotebook *notebook;
	GtkWidget *label;
	GList *children, *iter;
	gpointer order, child_order;
	int i;

	if (error) {
		gtk_widget_hide (editor->window);
		nm_connection_editor_error (editor->parent_window,
		                            _("Error initializing editor"),
		                            "%s", error->message);
		g_signal_emit (editor, editor_signals[EDITOR_DONE], 0, GTK_RESPONSE_NONE);
		return;
	}

	/* Add the page to the UI */
	notebook = GTK_NOTEBOOK (gtk_builder_get_object (editor->builder, "notebook"));
	label = gtk_label_new (ce_page_get_title (page));
	widget = ce_page_get_page (page);
	parent = gtk_widget_get_parent (widget);
	if (parent)
		gtk_container_remove (GTK_CONTAINER (parent), widget);

	order = g_object_get_data (G_OBJECT (page), ORDER_TAG);
	g_object_set_data (G_OBJECT (widget), ORDER_TAG, order);

	children = gtk_container_get_children (GTK_CONTAINER (notebook));
	for (iter = children, i = 0; iter; iter = iter->next, i++) {
		child_order = g_object_get_data (G_OBJECT (iter->data), ORDER_TAG);
		if (child_order > order)
			break;
	}
	g_list_free (children);

	gtk_notebook_insert_page (notebook, widget, label, i);

	if (CE_IS_PAGE_VPN (page) && ce_page_vpn_can_export (CE_PAGE_VPN (page)))
		gtk_widget_show (editor->export_button);

	/* Move the page from the initializing list to the main page list */
	editor->initializing_pages = g_slist_remove (editor->initializing_pages, page);
	editor->pages = g_slist_append (editor->pages, page);

	recheck_initialization (editor);
}

static void request_secrets (GetSecretsInfo *info);

static void
get_secrets_cb (GObject *object,
                GAsyncResult *result,
                gpointer user_data)
{
	NMRemoteConnection *connection = NM_REMOTE_CONNECTION (object);
	GetSecretsInfo *info = user_data;
	NMConnectionEditor *self;
	GVariant *secrets;
	GError *error = NULL;

	if (info->canceled) {
		get_secrets_info_free (info);
		return;
	}

	secrets = nm_remote_connection_get_secrets_finish (connection, result, &error);

	self = info->self;

	/* Complete this secrets request; completion can actually dispose of the
	 * dialog if there was an error.
	 */
	self->secrets_call = NULL;
	ce_page_complete_init (info->page, info->setting_name, secrets, error);
	get_secrets_info_free (info);

	/* Kick off the next secrets request if there is one queued; if the dialog
	 * was disposed of by the completion above we don't need to do anything.
	 */
	if (!self->disposed && self->pending_secrets_calls) {
		self->secrets_call = g_slist_nth_data (self->pending_secrets_calls, 0);
		self->pending_secrets_calls = g_slist_remove (self->pending_secrets_calls, self->secrets_call);

		request_secrets (self->secrets_call);
	}
}

static void
request_secrets (GetSecretsInfo *info)
{
	g_return_if_fail (info != NULL);

	nm_remote_connection_get_secrets_async (NM_REMOTE_CONNECTION (info->self->orig_connection),
	                                        info->setting_name, NULL, get_secrets_cb, info);
}

static void
get_secrets_for_page (NMConnectionEditor *self,
                      CEPage *page,
                      const char *setting_name)
{
	GetSecretsInfo *info;

	info = g_malloc0 (sizeof (GetSecretsInfo));
	info->self = self;
	info->page = page;
	info->setting_name = g_strdup (setting_name);

	/* PolicyKit doesn't queue up authorization requests internally.  Instead,
	 * if there's a pending authorization request, subsequent requests for that
	 * same authorization will return NotAuthorized+Challenge.  That's pretty
	 * inconvenient and it would be a lot nicer if PK just queued up subsequent
	 * authorization requests and executed them when the first one was finished.
	 * But it since it doesn't do that, we have to serialize the authorization
	 * requests ourselves to get the right authorization result.
	 */
	/* NOTE: PolicyKit-gnome 0.95 now serializes auth requests as of this commit:
	 * http://git.gnome.org/cgit/PolicyKit-gnome/commit/?id=f32cb7faa7197b9db55b569677732742c3c7fdc1
	 */

	/* If there's already an in-progress call, queue up the new one */
	if (self->secrets_call)
		self->pending_secrets_calls = g_slist_append (self->pending_secrets_calls, info);
	else {
		/* Request secrets for this page */
		self->secrets_call = info;
		request_secrets (info);
	}
}

static gboolean
add_page (NMConnectionEditor *editor,
          CEPageNewFunc func,
          NMConnection *connection,
          GError **error)
{
	CEPage *page;
	const char *secrets_setting_name = NULL;

	g_return_val_if_fail (editor != NULL, FALSE);
	g_return_val_if_fail (func != NULL, FALSE);
	g_return_val_if_fail (connection != NULL, FALSE);

	page = (*func) (editor, connection, GTK_WINDOW (editor->window), editor->client,
	                &secrets_setting_name, error);
	if (page) {
		g_object_set_data_full (G_OBJECT (page),
		                        SECRETS_TAG,
		                        g_strdup (secrets_setting_name),
		                        g_free);
		g_object_set_data (G_OBJECT (page),
		                   ORDER_TAG,
		                   GINT_TO_POINTER (g_slist_length (editor->initializing_pages)));

		editor->initializing_pages = g_slist_append (editor->initializing_pages, page);
		g_signal_connect (page, "changed", G_CALLBACK (page_changed), editor);
		g_signal_connect (page, "initialized", G_CALLBACK (page_initialized), editor);
	}
	return !!page;
}

static gboolean
nm_connection_editor_set_connection (NMConnectionEditor *editor,
                                     NMConnection *orig_connection,
                                     GError **error)
{
	NMSettingConnection *s_con;
	const char *connection_type;
	const char *slave_type;
	gboolean success = FALSE;
	GSList *iter, *copy;

	g_return_val_if_fail (NM_IS_CONNECTION_EDITOR (editor), FALSE);
	g_return_val_if_fail (NM_IS_CONNECTION (orig_connection), FALSE);

	/* clean previous connection */
	if (editor->connection)
		g_object_unref (editor->connection);

	editor->connection = nm_simple_connection_new_clone (orig_connection);

	editor->orig_connection = g_object_ref (orig_connection);
	nm_connection_editor_update_title (editor);

	/* Handle CA cert ignore stuff */
	eap_method_ca_cert_ignore_load (editor->connection);

	s_con = nm_connection_get_setting_connection (editor->connection);
	g_assert (s_con);

	connection_type = nm_setting_connection_get_connection_type (s_con);
	if (!add_page (editor, ce_page_general_new, editor->connection, error))
		goto out;
	if (!strcmp (connection_type, NM_SETTING_WIRED_SETTING_NAME)) {
		if (!add_page (editor, ce_page_ethernet_new, editor->connection, error))
			goto out;
		if (!add_page (editor, ce_page_8021x_security_new, editor->connection, error))
			goto out;
		if (!add_page (editor, ce_page_dcb_new, editor->connection, error))
			goto out;
	} else if (!strcmp (connection_type, NM_SETTING_WIRELESS_SETTING_NAME)) {
		if (!add_page (editor, ce_page_wifi_new, editor->connection, error))
			goto out;
		if (!add_page (editor, ce_page_wifi_security_new, editor->connection, error))
			goto out;
	} else if (!strcmp (connection_type, NM_SETTING_VPN_SETTING_NAME)) {
		if (!add_page (editor, ce_page_vpn_new, editor->connection, error))
			goto out;
	} else if (!strcmp (connection_type, NM_SETTING_PPPOE_SETTING_NAME)) {
		if (!add_page (editor, ce_page_dsl_new, editor->connection, error))
			goto out;
		if (!add_page (editor, ce_page_ethernet_new, editor->connection, error))
			goto out;
		if (!add_page (editor, ce_page_ppp_new, editor->connection, error))
			goto out;
	} else if (!strcmp (connection_type, NM_SETTING_GSM_SETTING_NAME) || 
	           !strcmp (connection_type, NM_SETTING_CDMA_SETTING_NAME)) {
		if (!add_page (editor, ce_page_mobile_new, editor->connection, error))
			goto out;
		if (!add_page (editor, ce_page_ppp_new, editor->connection, error))
			goto out;
	} else if (!strcmp (connection_type, NM_SETTING_BLUETOOTH_SETTING_NAME)) {
		NMSettingBluetooth *s_bt = nm_connection_get_setting_bluetooth (editor->connection);
		const char *type = nm_setting_bluetooth_get_connection_type (s_bt);
		g_assert (type);

		if (!add_page (editor, ce_page_bluetooth_new, editor->connection, error))
			goto out;
		if (!g_strcmp0 (type, "dun")) {
			if (!add_page (editor, ce_page_mobile_new, editor->connection, error))
				goto out;
			if (!add_page (editor, ce_page_ppp_new, editor->connection, error))
				goto out;
		}
	} else if (!strcmp (connection_type, NM_SETTING_INFINIBAND_SETTING_NAME)) {
		if (!add_page (editor, ce_page_infiniband_new, editor->connection, error))
			goto out;
	} else if (!strcmp (connection_type, NM_SETTING_BOND_SETTING_NAME)) {
		if (!add_page (editor, ce_page_bond_new, editor->connection, error))
			goto out;
	} else if (!strcmp (connection_type, NM_SETTING_TEAM_SETTING_NAME)) {
		if (!add_page (editor, ce_page_team_new, editor->connection, error))
			goto out;
	} else if (!strcmp (connection_type, NM_SETTING_BRIDGE_SETTING_NAME)) {
		if (!add_page (editor, ce_page_bridge_new, editor->connection, error))
			goto out;
	} else if (!strcmp (connection_type, NM_SETTING_VLAN_SETTING_NAME)) {
		if (!add_page (editor, ce_page_vlan_new, editor->connection, error))
			goto out;
	} else {
		g_warning ("Unhandled setting type '%s'", connection_type);
	}

	slave_type = nm_setting_connection_get_slave_type (s_con);
	if (!g_strcmp0 (slave_type, NM_SETTING_TEAM_SETTING_NAME)) {
		if (!add_page (editor, ce_page_team_port_new, editor->connection, error))
			goto out;
	} else if (!g_strcmp0 (slave_type, NM_SETTING_BRIDGE_SETTING_NAME)) {
		if (!add_page (editor, ce_page_bridge_port_new, editor->connection, error))
			goto out;
	}

	if (   nm_connection_get_setting_ip4_config (editor->connection)
	    && !add_page (editor, ce_page_ip4_new, editor->connection, error))
		goto out;
	if (   nm_connection_get_setting_ip6_config (editor->connection)
	    && !add_page (editor, ce_page_ip6_new, editor->connection, error))
		goto out;

	/* After all pages are created, then kick off secrets requests that any
	 * the pages may need to make; if they don't need any secrets, then let
	 * them finish initialization.  The list might get modified during the loop
	 * which is why copy the list here.
	 */
	copy = g_slist_copy (editor->initializing_pages);
	for (iter = copy; iter; iter = g_slist_next (iter)) {
		CEPage *page = CE_PAGE (iter->data);
		const char *setting_name = g_object_get_data (G_OBJECT (page), SECRETS_TAG);

		if (!setting_name) {
			/* page doesn't need any secrets */
			ce_page_complete_init (page, NULL, NULL, NULL);
		} else if (!NM_IS_REMOTE_CONNECTION (editor->orig_connection)) {
			/* We want to get secrets using ->orig_connection, since that's the
			 * remote connection which can actually respond to secrets requests.
			 * ->connection is a plain NMConnection copy of ->orig_connection
			 * which is what gets changed when users modify anything.  But when
			 * creating or importing, ->orig_connection will be an NMConnection
			 * since the new connection hasn't been added to NetworkManager yet.
			 * So basically, skip requesting secrets if the connection can't
			 * handle a secrets request.
			 */
			ce_page_complete_init (page, setting_name, NULL, NULL);
		} else {
			/* Page wants secrets, get them */
			get_secrets_for_page (editor, page, setting_name);
		}
		g_object_set_data (G_OBJECT (page), SECRETS_TAG, NULL);
	}
	g_slist_free (copy);

	/* set the UI */
	recheck_initialization (editor);
	success = TRUE;

out:
	return success;
}

void
nm_connection_editor_present (NMConnectionEditor *editor)
{
	g_return_if_fail (NM_IS_CONNECTION_EDITOR (editor));

	gtk_window_present (GTK_WINDOW (editor->window));
}

static void
cancel_button_clicked_cb (GtkWidget *widget, gpointer user_data)
{
	NMConnectionEditor *self = NM_CONNECTION_EDITOR (user_data);

	/* If the dialog is busy waiting for authorization or something,
	 * don't destroy it until authorization returns.
	 */
	if (self->busy)
		return;

	g_signal_emit (self, editor_signals[EDITOR_DONE], 0, GTK_RESPONSE_CANCEL);
}

static void
editor_closed_cb (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	cancel_button_clicked_cb (widget, user_data);
}

static void
added_connection_cb (GObject *client,
                     GAsyncResult *result,
                     gpointer user_data)
{
	NMConnectionEditor *self = user_data;
	NMRemoteConnection *connection;
	GError *error = NULL;

	nm_connection_editor_set_busy (self, FALSE);

	connection = nm_client_add_connection_finish (NM_CLIENT (client), result, &error);
	if (error) {
		nm_connection_editor_error (self->parent_window, _("Connection add failed"),
		                            "%s", error->message);
		/* Leave the editor open */
		return;
	}
	g_clear_object (&connection);
	g_clear_error (&error);

	g_signal_emit (self, editor_signals[EDITOR_DONE], 0, GTK_RESPONSE_OK);
}

static void
update_complete (NMConnectionEditor *self, GError *error)
{
	nm_connection_editor_set_busy (self, FALSE);
	g_signal_emit (self, editor_signals[EDITOR_DONE], 0, GTK_RESPONSE_OK);
}

static void
updated_connection_cb (GObject *connection,
                       GAsyncResult *result,
                       gpointer user_data)
{
	NMConnectionEditor *self = NM_CONNECTION_EDITOR (user_data);
	GError *error = NULL;

	nm_remote_connection_commit_changes_finish (NM_REMOTE_CONNECTION (connection),
	                                            result, &error);

	/* Clear secrets so they don't lay around in memory; they'll get requested
	 * again anyway next time the connection is edited.
	 */
	nm_connection_clear_secrets (NM_CONNECTION (connection));

	update_complete (self, error);
	g_clear_error (&error);
}

static void
ok_button_clicked_save_connection (NMConnectionEditor *self)
{
	/* Copy the modified connection to the original connection */
	nm_connection_replace_settings_from_connection (self->orig_connection,
	                                                self->connection);
	nm_connection_editor_set_busy (self, TRUE);

	/* Save new CA cert ignore values to GSettings */
	eap_method_ca_cert_ignore_save (self->connection);

	if (self->is_new_connection) {
		nm_client_add_connection_async (self->client,
		                                self->orig_connection,
		                                TRUE,
		                                NULL,
		                                added_connection_cb,
		                                self);
	} else {
		nm_remote_connection_commit_changes_async (NM_REMOTE_CONNECTION (self->orig_connection),
		                                           TRUE, NULL, updated_connection_cb, self);
	}
}

static void
ok_button_clicked_cb (GtkWidget *widget, gpointer user_data)
{
	NMConnectionEditor *self = NM_CONNECTION_EDITOR (user_data);
	GSList *iter;

	/* If the dialog is busy waiting for authorization or something,
	 * don't destroy it until authorization returns.
	 */
	if (self->busy)
		return;

	/* Validate one last time to ensure all pages update the connection */
	connection_editor_validate (self);

	/* Perform page specific actions before the connection is saved */
	for (iter = self->pages; iter; iter = g_slist_next (iter))
		ce_page_last_update (CE_PAGE (iter->data), self->connection, NULL);

	ok_button_clicked_save_connection (self);
}

static void
vpn_export_get_secrets_cb (GObject *object,
                           GAsyncResult *result,
                           gpointer user_data)
{
	NMConnection *tmp;
	GVariant *secrets;
	GError *error = NULL;

	secrets = nm_remote_connection_get_secrets_finish (NM_REMOTE_CONNECTION (object),
	                                                   result, &error);

	/* We don't really care about errors; if the user couldn't authenticate
	 * then just let them export everything except secrets.  Duplicate the
	 * connection so that we don't let secrets sit around in the original
	 * one.
	 */
	tmp = nm_simple_connection_new_clone (NM_CONNECTION (object));
	g_assert (tmp);
	if (secrets)
		nm_connection_update_secrets (tmp, NM_SETTING_VPN_SETTING_NAME, secrets, NULL);
	vpn_export (tmp);
	g_object_unref (tmp);
	if (secrets)
		g_variant_ref (secrets);
	g_clear_error (&error);
}

static void
export_button_clicked_cb (GtkWidget *widget, gpointer user_data)
{
	NMConnectionEditor *self = NM_CONNECTION_EDITOR (user_data);

	if (NM_IS_REMOTE_CONNECTION (self->orig_connection)) {
		/* Grab secrets if we can */
		nm_remote_connection_get_secrets_async (NM_REMOTE_CONNECTION (self->orig_connection),
		                                        NM_SETTING_VPN_SETTING_NAME,
		                                        NULL,
		                                        vpn_export_get_secrets_cb,
		                                        self);
	} else
		vpn_export (self->connection);
}

void
nm_connection_editor_run (NMConnectionEditor *self)
{
	g_return_if_fail (NM_IS_CONNECTION_EDITOR (self));

	g_signal_connect (G_OBJECT (self->window), "delete-event",
	                  G_CALLBACK (editor_closed_cb), self);

	g_signal_connect (G_OBJECT (self->ok_button), "clicked",
	                  G_CALLBACK (ok_button_clicked_cb), self);
	g_signal_connect (G_OBJECT (self->cancel_button), "clicked",
	                  G_CALLBACK (cancel_button_clicked_cb), self);
	g_signal_connect (G_OBJECT (self->export_button), "clicked",
	                  G_CALLBACK (export_button_clicked_cb), self);

	nm_connection_editor_present (self);
}

GtkWindow *
nm_connection_editor_get_window (NMConnectionEditor *editor)
{
	g_return_val_if_fail (NM_IS_CONNECTION_EDITOR (editor), NULL);

	return GTK_WINDOW (editor->window);
}

gboolean
nm_connection_editor_get_busy (NMConnectionEditor *editor)
{
	g_return_val_if_fail (NM_IS_CONNECTION_EDITOR (editor), FALSE);

	return editor->busy;
}

void
nm_connection_editor_set_busy (NMConnectionEditor *editor, gboolean busy)
{
	g_return_if_fail (NM_IS_CONNECTION_EDITOR (editor));

	if (busy != editor->busy) {
		editor->busy = busy;
		gtk_widget_set_sensitive (editor->window, !busy);
	}
}

static void
nm_connection_editor_dialog (GtkWindow *parent, GtkMessageType type, const char *heading,
                             const char *format, va_list args)
{
	GtkWidget *dialog;
	char *message;

	dialog = gtk_message_dialog_new (parent,
	                                 GTK_DIALOG_DESTROY_WITH_PARENT,
	                                 type,
	                                 GTK_BUTTONS_CLOSE,
	                                 "%s", heading);

	message = g_strdup_vprintf (format, args);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", message);
	g_free (message);

	gtk_widget_show_all (dialog);
	gtk_window_present (GTK_WINDOW (dialog));
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

void
nm_connection_editor_error (GtkWindow *parent, const char *heading, const char *format, ...)
{
	va_list args;

	va_start (args, format);
	nm_connection_editor_dialog (parent, GTK_MESSAGE_ERROR, heading, format, args);
	va_end (args);
}

void
nm_connection_editor_warning (GtkWindow *parent, const char *heading, const char *format, ...)
{
	va_list args;

	va_start (args, format);
	nm_connection_editor_dialog (parent, GTK_MESSAGE_WARNING, heading, format, args);
	va_end (args);
}

void
nm_connection_editor_inter_page_set_value (NMConnectionEditor *editor, InterPageChangeType type, gpointer value)
{
	g_hash_table_insert (editor->inter_page_hash, GUINT_TO_POINTER (type), value);
}

gboolean
nm_connection_editor_inter_page_get_value (NMConnectionEditor *editor, InterPageChangeType type, gpointer *value)
{
	return g_hash_table_lookup_extended (editor->inter_page_hash, GUINT_TO_POINTER (type), NULL, value);
}

void
nm_connection_editor_inter_page_clear_data (NMConnectionEditor *editor)
{
	g_hash_table_remove_all (editor->inter_page_hash);
}

