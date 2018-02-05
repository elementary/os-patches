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
 * (C) Copyright 2016 Atul Anand <atulhjp@gmail.com>.
 */

#include "nm-default.h"

#include "page-proxy.h"

#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "nm-connection-editor.h"

G_DEFINE_TYPE (CEPageProxy, ce_page_proxy, CE_TYPE_PAGE)

#define CE_PAGE_PROXY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_PROXY, CEPageProxyPrivate))

typedef struct {
	NMSettingProxy *setting;

	/* Method */
	GtkComboBox *method;

	/* Browser Only */
	GtkCheckButton *browser_only;

	/* PAC URL */
	GtkWidget *pac_url_label;
	GtkEntry *pac_url;

	/* PAC Script */
	GtkWidget *pac_script_label;
	GtkButton *pac_script_import_button;
	GtkTextView *pac_script_window;
} CEPageProxyPrivate;

#define PROXY_METHOD_NONE    0
#define PROXY_METHOD_AUTO    1

static void
proxy_private_init (CEPageProxy *self)
{
	CEPageProxyPrivate *priv = CE_PAGE_PROXY_GET_PRIVATE (self);
	GtkBuilder *builder;

	builder = CE_PAGE (self)->builder;

	priv->method = GTK_COMBO_BOX (gtk_builder_get_object (builder, "proxy_method"));

	priv->browser_only = GTK_CHECK_BUTTON (gtk_builder_get_object (builder, "proxy_browser_only_checkbutton"));

	priv->pac_url_label = GTK_WIDGET (gtk_builder_get_object (builder, "proxy_pac_url_label"));
	priv->pac_url = GTK_ENTRY (gtk_builder_get_object (builder, "proxy_pac_url_entry"));

	priv->pac_script_label = GTK_WIDGET (gtk_builder_get_object (builder, "proxy_pac_script_label"));
	priv->pac_script_import_button = GTK_BUTTON (gtk_builder_get_object (builder, "proxy_pac_script_import_button"));
	priv->pac_script_window = GTK_TEXT_VIEW (gtk_builder_get_object (builder, "proxy_pac_script_window"));
}

static void
stuff_changed (GtkWidget *w, gpointer user_data)
{
	ce_page_changed (CE_PAGE (user_data));
}

static void
method_changed (GtkComboBox *combo, gpointer user_data)
{
	CEPageProxy *self = user_data;
	CEPageProxyPrivate *priv = CE_PAGE_PROXY_GET_PRIVATE (self);
	int method = gtk_combo_box_get_active (combo);

	if (method == PROXY_METHOD_NONE) {
		gtk_widget_set_sensitive (GTK_WIDGET (priv->pac_url_label), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->pac_url), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->pac_script_label), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->pac_script_import_button), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->pac_script_window), FALSE);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (priv->pac_url_label), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->pac_url), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->pac_script_label), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->pac_script_import_button), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->pac_script_window), TRUE);
	}
}

static void
import_button_clicked_cb (GtkWidget *widget, CEPageProxy *self)
{
	CEPageProxyPrivate *priv = CE_PAGE_PROXY_GET_PRIVATE (self);
	GtkWidget *dialog, *toplevel;
	GtkTextBuffer *buffer;
	char *filename, *script = NULL;
	gsize len;

	toplevel = gtk_widget_get_toplevel (CE_PAGE (self)->page);
	g_return_if_fail (toplevel);
	g_return_if_fail (gtk_widget_is_toplevel (toplevel));

	dialog = gtk_file_chooser_dialog_new (_("Select file to import"),
	                                      GTK_WINDOW (toplevel),
	                                      GTK_FILE_CHOOSER_ACTION_OPEN,
	                                      _("_Cancel"), GTK_RESPONSE_CANCEL,
	                                      _("_Open"), GTK_RESPONSE_ACCEPT,
	                                      NULL);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		if (!filename)
			goto out;

		g_file_get_contents (filename, &script, &len, NULL);
		buffer = gtk_text_view_get_buffer (priv->pac_script_window);
		gtk_text_buffer_set_text (buffer, script ?: "", -1);

		g_free (filename);
		g_free (script);
	}

out:
	gtk_widget_destroy (dialog);
}

static void
populate_ui (CEPageProxy *self)
{
	CEPageProxyPrivate *priv = CE_PAGE_PROXY_GET_PRIVATE (self);
	NMSettingProxy *setting = priv->setting;
	NMSettingProxyMethod s_method;
	GtkTextBuffer *buffer;
	const char *tmp = NULL;

	/* Method */
	s_method = nm_setting_proxy_get_method (setting);
	switch (s_method) {
	case NM_SETTING_PROXY_METHOD_AUTO:
		gtk_combo_box_set_active (priv->method, PROXY_METHOD_AUTO);

		/* Pac Url */
		tmp = nm_setting_proxy_get_pac_url (setting);
		gtk_entry_set_text (priv->pac_url, tmp ? tmp : "");

		/* Pac Script */
		tmp = nm_setting_proxy_get_pac_script (setting);
		buffer = gtk_text_view_get_buffer (priv->pac_script_window);
		gtk_text_buffer_set_text (buffer, tmp ?: "", -1);
		break;
	case NM_SETTING_PROXY_METHOD_NONE:
		gtk_combo_box_set_active (priv->method, PROXY_METHOD_NONE);
		/* Nothing to Show */
	}

	g_signal_connect (priv->method, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->browser_only, "toggled", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->pac_url, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (gtk_text_view_get_buffer (priv->pac_script_window), "changed", G_CALLBACK (stuff_changed), self);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->browser_only),
	                              nm_setting_proxy_get_browser_only (setting));
}

static void
finish_setup (CEPageProxy *self, gpointer unused, GError *error, gpointer user_data)
{
	CEPageProxyPrivate *priv = CE_PAGE_PROXY_GET_PRIVATE (self);

	if (error)
		return;

	populate_ui (self);

	method_changed (priv->method, self);
	g_signal_connect (priv->method, "changed", G_CALLBACK (method_changed), self);
	g_signal_connect (priv->pac_script_import_button, "clicked", G_CALLBACK (import_button_clicked_cb), self);
}

CEPage *
ce_page_proxy_new (NMConnectionEditor *editor,
                   NMConnection *connection,
                   GtkWindow *parent_window,
                   NMClient *client,
                   const char **out_secrets_setting_name,
                   GError **error)
{
	CEPageProxy *self;
	CEPageProxyPrivate *priv;
	NMSettingConnection *s_con;

	self = CE_PAGE_PROXY (ce_page_new (CE_TYPE_PAGE_PROXY,
	                                   editor,
	                                   connection,
	                                   parent_window,
	                                   client,
	                                   "/org/freedesktop/network-manager-applet/ce-page-proxy.ui",
	                                   "ProxyPage",
	                                   _("Proxy")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load proxy user interface."));
		return NULL;
	}

	proxy_private_init (self);
	priv = CE_PAGE_PROXY_GET_PRIVATE (self);

	s_con = nm_connection_get_setting_connection (connection);
	g_assert (s_con);

	priv->setting = nm_connection_get_setting_proxy (connection);
	g_assert (priv->setting);

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return CE_PAGE (self);
}

static void
ui_to_setting (CEPageProxy *self)
{
	CEPageProxyPrivate *priv = CE_PAGE_PROXY_GET_PRIVATE (self);
	NMSettingConnection *s_con;
	int method;
	gboolean browser_only;
	const char *pac_url;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	char *script = NULL;

	s_con = nm_connection_get_setting_connection (CE_PAGE (self)->connection);
	g_return_if_fail (s_con != NULL);

	/* Method */
	method = gtk_combo_box_get_active (priv->method);

	/* Browser Only */
	browser_only = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->browser_only));

	if (method == PROXY_METHOD_NONE) {
		g_object_set (priv->setting,
		              NM_SETTING_PROXY_METHOD, NM_SETTING_PROXY_METHOD_NONE,
		              NM_SETTING_PROXY_BROWSER_ONLY, browser_only,
		              NM_SETTING_PROXY_PAC_URL, NULL,
		              NM_SETTING_PROXY_PAC_SCRIPT, NULL,
		              NULL);
		return;
	}

	/* PAC Url */
	pac_url = gtk_entry_get_text (priv->pac_url);
	if (pac_url && strlen (pac_url) < 1)
		pac_url = NULL;

	/* PAC Script */
	buffer = gtk_text_view_get_buffer (priv->pac_script_window);
	gtk_text_buffer_get_iter_at_offset (buffer, &start, 0);
	gtk_text_buffer_get_iter_at_offset (buffer, &end, -1);
	script = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	/* Update NMSetting */
	g_object_set (priv->setting,
	              NM_SETTING_PROXY_METHOD, NM_SETTING_PROXY_METHOD_AUTO,
	              NM_SETTING_PROXY_BROWSER_ONLY, browser_only,
	              NM_SETTING_PROXY_PAC_URL, pac_url,
	              NM_SETTING_PROXY_PAC_SCRIPT, nm_str_not_empty (script),
	              NULL);
	g_free (script);
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageProxy *self = CE_PAGE_PROXY (page);
	CEPageProxyPrivate *priv = CE_PAGE_PROXY_GET_PRIVATE (self);

	if (!priv->setting) {
		priv->setting = (NMSettingProxy *) nm_setting_proxy_new ();
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}
	ui_to_setting (self);

	return nm_setting_verify (NM_SETTING (priv->setting), NULL, error);
}

static void
ce_page_proxy_init (CEPageProxy *self)
{
}

static void
ce_page_proxy_class_init (CEPageProxyClass *proxy_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (proxy_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (proxy_class);

	g_type_class_add_private (object_class, sizeof (CEPageProxyPrivate));

	/* virtual methods */
	parent_class->ce_page_validate_v = ce_page_validate_v;
}
