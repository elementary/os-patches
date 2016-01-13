/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2013 Jiri Pirko <jiri@resnulli.us>
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
 */

#include "config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <nm-setting-connection.h>
#include <nm-setting-team-port.h>
#include <nm-utils.h>

#include "page-team-port.h"

G_DEFINE_TYPE (CEPageTeamPort, ce_page_team_port, CE_TYPE_PAGE)

#define CE_PAGE_TEAM_PORT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_TEAM_PORT, CEPageTeamPortPrivate))

typedef struct {
	NMSettingTeamPort *setting;

	GtkTextView *json_config_widget;
	GtkWidget *import_config_button;
} CEPageTeamPortPrivate;

static void
team_port_private_init (CEPageTeamPort *self)
{
	CEPageTeamPortPrivate *priv = CE_PAGE_TEAM_PORT_GET_PRIVATE (self);
	GtkBuilder *builder;

	builder = CE_PAGE (self)->builder;

	priv->json_config_widget = GTK_TEXT_VIEW (gtk_builder_get_object (builder, "team_port_json_config"));
	priv->import_config_button = GTK_WIDGET (gtk_builder_get_object (builder, "import_config_button"));
}

static void
json_config_changed (GObject *object, CEPageTeamPort *self)
{
	ce_page_changed (CE_PAGE (self));
}

static void
import_button_clicked_cb (GtkWidget *widget, CEPageTeamPort *self)
{
	CEPageTeamPortPrivate *priv = CE_PAGE_TEAM_PORT_GET_PRIVATE (self);
	GtkWidget *dialog;
	GtkWindow *toplevel;
	GtkTextBuffer *buffer;
	char *filename;
	char *buf = NULL;
	gsize buf_len;

	toplevel = GTK_WINDOW (gtk_widget_get_toplevel (widget));
	if (!gtk_widget_is_toplevel (GTK_WIDGET (toplevel)))
		toplevel = NULL;

	dialog = gtk_file_chooser_dialog_new (_("Select file to import"),
	                                      toplevel,
	                                      GTK_FILE_CHOOSER_ACTION_OPEN,
	                                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
	                                      NULL);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		if (!filename) {
			g_warning ("%s: didn't get a filename back from the chooser!", __func__);
			goto out;
		}

		/* Put the file content into JSON config text view. */
		// FIXME: do a cleverer file validity check
		g_file_get_contents (filename, &buf, &buf_len, NULL);
		if (buf_len > 100000) {
			g_free (buf);
			buf = g_strdup (_("Error: file doesn't contain a valid JSON configuration"));
		}

		buffer = gtk_text_view_get_buffer (priv->json_config_widget);
		gtk_text_buffer_set_text (buffer, buf ? buf : "", -1);

		g_free (filename);
		g_free (buf);
	}

out:
	gtk_widget_destroy (dialog);
}

static void
populate_ui (CEPageTeamPort *self)
{
	CEPageTeamPortPrivate *priv = CE_PAGE_TEAM_PORT_GET_PRIVATE (self);
	NMSettingTeamPort *s_port = priv->setting;
	GtkTextBuffer *buffer;
	const char *json_config;

	buffer = gtk_text_view_get_buffer (priv->json_config_widget);
	json_config = nm_setting_team_port_get_config (s_port);
	gtk_text_buffer_set_text (buffer, json_config ? json_config : "", -1);

	g_signal_connect (buffer, "changed", G_CALLBACK (json_config_changed), self);
	g_signal_connect (priv->import_config_button, "clicked", G_CALLBACK (import_button_clicked_cb), self);
}

static void
finish_setup (CEPageTeamPort *self, gpointer unused, GError *error, gpointer user_data)
{
	if (error)
		return;

	populate_ui (self);
}

CEPage *
ce_page_team_port_new (NMConnectionEditor *editor,
                       NMConnection *connection,
                       GtkWindow *parent_window,
                       NMClient *client,
                       NMRemoteSettings *settings,
                       const char **out_secrets_setting_name,
                       GError **error)
{
	CEPageTeamPort *self;
	CEPageTeamPortPrivate *priv;

	self = CE_PAGE_TEAM_PORT (ce_page_new (CE_TYPE_PAGE_TEAM_PORT,
	                                       editor,
	                                       connection,
	                                       parent_window,
	                                       client,
	                                       settings,
	                                       UIDIR "/ce-page-team-port.ui",
	                                       "TeamPortPage",
	                                       /* Translators: a "Team Port" is a network
	                                        * device that is part of a team.
	                                        */
	                                       _("Team Port")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load team port user interface."));
		return NULL;
	}

	team_port_private_init (self);
	priv = CE_PAGE_TEAM_PORT_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_team_port (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_TEAM_PORT (nm_setting_team_port_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return CE_PAGE (self);
}

static void
ui_to_setting (CEPageTeamPort *self)
{
	CEPageTeamPortPrivate *priv = CE_PAGE_TEAM_PORT_GET_PRIVATE (self);
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	char *json_config;

	buffer = gtk_text_view_get_buffer (priv->json_config_widget);
	gtk_text_buffer_get_iter_at_offset (buffer, &start, 0);
	gtk_text_buffer_get_iter_at_offset (buffer, &end, -1);
	json_config = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	if (g_strcmp0 (json_config, "") == 0)
		json_config = NULL;
	g_object_set (priv->setting,
	              NM_SETTING_TEAM_PORT_CONFIG, json_config,
	              NULL);
	g_free (json_config);
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageTeamPort *self = CE_PAGE_TEAM_PORT (page);
	CEPageTeamPortPrivate *priv = CE_PAGE_TEAM_PORT_GET_PRIVATE (self);

	ui_to_setting (self);
	return nm_setting_verify (NM_SETTING (priv->setting), NULL, error);
}

static void
ce_page_team_port_init (CEPageTeamPort *self)
{
}

static void
ce_page_team_port_class_init (CEPageTeamPortClass *team_port_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (team_port_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (team_port_class);

	g_type_class_add_private (object_class, sizeof (CEPageTeamPortPrivate));

	/* virtual methods */
	parent_class->ce_page_validate_v = ce_page_validate_v;
}
