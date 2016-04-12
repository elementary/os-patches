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
 * Copyright 2008 - 2014 Red Hat, Inc.
 */

#include "nm-default.h"

#include <string.h>

#include "wireless-security.h"
#include "page-ethernet.h"
#include "page-8021x-security.h"
#include "nm-connection-editor.h"

G_DEFINE_TYPE (CEPage8021xSecurity, ce_page_8021x_security, CE_TYPE_PAGE)

#define CE_PAGE_8021X_SECURITY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_8021X_SECURITY, CEPage8021xSecurityPrivate))

typedef struct {
	GtkToggleButton *enabled;
	GtkWidget *security_widget;
	WirelessSecurity *security;

	gboolean initial_have_8021x;
} CEPage8021xSecurityPrivate;

static void
stuff_changed (WirelessSecurity *sec, gpointer user_data)
{
	ce_page_changed (CE_PAGE (user_data));
}

static void
enable_toggled (GtkToggleButton *button, gpointer user_data)
{
	CEPage8021xSecurityPrivate *priv = CE_PAGE_8021X_SECURITY_GET_PRIVATE (user_data);

	gtk_widget_set_sensitive (priv->security_widget, gtk_toggle_button_get_active (priv->enabled));
	ce_page_changed (CE_PAGE (user_data));
}

static void
finish_setup (CEPage8021xSecurity *self, gpointer unused, GError *error, gpointer user_data)
{
	CEPage *parent = CE_PAGE (self);
	CEPage8021xSecurityPrivate *priv = CE_PAGE_8021X_SECURITY_GET_PRIVATE (self);
	GtkWidget *parent_container;

	if (error)
		return;

	priv->security = (WirelessSecurity *) ws_wpa_eap_new (parent->connection, TRUE, FALSE);
	if (!priv->security) {
		g_warning ("Could not load 802.1x user interface.");
		return;
	}

	wireless_security_set_changed_notify (priv->security, stuff_changed, self);
	priv->security_widget = wireless_security_get_widget (priv->security);
	parent_container = gtk_widget_get_parent (priv->security_widget);
	if (parent_container)
		gtk_container_remove (GTK_CONTAINER (parent_container), priv->security_widget);

	gtk_toggle_button_set_active (priv->enabled, priv->initial_have_8021x);
	g_signal_connect (priv->enabled, "toggled", G_CALLBACK (enable_toggled), self);
	gtk_widget_set_sensitive (priv->security_widget, priv->initial_have_8021x);

	gtk_box_pack_start (GTK_BOX (parent->page), GTK_WIDGET (priv->enabled), FALSE, TRUE, 12);
	gtk_box_pack_start (GTK_BOX (parent->page), priv->security_widget, TRUE, TRUE, 0);
	gtk_widget_show_all (parent->page);
}

CEPage *
ce_page_8021x_security_new (NMConnectionEditor *editor,
                            NMConnection *connection,
                            GtkWindow *parent_window,
                            NMClient *client,
                            const char **out_secrets_setting_name,
                            GError **error)
{
	CEPage8021xSecurity *self;
	CEPage8021xSecurityPrivate *priv;
	CEPage *parent;

	self = CE_PAGE_8021X_SECURITY (ce_page_new (CE_TYPE_PAGE_8021X_SECURITY,
	                                            editor,
	                                            connection,
	                                            parent_window,
	                                            client,
	                                            NULL,
	                                            NULL,
	                                            _("802.1x Security")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load 802.1x Security user interface."));
		return NULL;
	}

	parent = CE_PAGE (self);
	priv = CE_PAGE_8021X_SECURITY_GET_PRIVATE (self);

	parent->page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	g_object_ref_sink (G_OBJECT (parent->page));
	gtk_container_set_border_width (GTK_CONTAINER (parent->page), 6);

	if (nm_connection_get_setting_802_1x (connection))
		priv->initial_have_8021x = TRUE;

	priv->enabled = GTK_TOGGLE_BUTTON (gtk_check_button_new_with_mnemonic (_("Use 802.1_X security for this connection")));

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	if (priv->initial_have_8021x)
		*out_secrets_setting_name = NM_SETTING_802_1X_SETTING_NAME;

	return CE_PAGE (self);
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPage8021xSecurityPrivate *priv = CE_PAGE_8021X_SECURITY_GET_PRIVATE (page);
	gboolean valid = TRUE;

	if (gtk_toggle_button_get_active (priv->enabled)) {
		NMConnection *tmp_connection;
		NMSetting *s_8021x;

		valid = wireless_security_validate (priv->security, error);
		if (valid) {
			NMSetting *s_con;

			/* Here's a nice hack to work around the fact that ws_802_1x_fill_connection needs wireless setting. */
			tmp_connection = nm_simple_connection_new ();
			nm_connection_add_setting (tmp_connection, nm_setting_wireless_new ());

			/* temp connection needs a 'connection' setting too, since most of
			 * the EAP methods need the UUID for CA cert ignore stuff.
			 */
			s_con = nm_connection_get_setting (connection, NM_TYPE_SETTING_CONNECTION);
			nm_connection_add_setting (tmp_connection, nm_setting_duplicate (s_con));

			ws_802_1x_fill_connection (priv->security, "wpa_eap_auth_combo", tmp_connection);

			s_8021x = nm_connection_get_setting (tmp_connection, NM_TYPE_SETTING_802_1X);
			nm_connection_add_setting (connection, NM_SETTING (g_object_ref (s_8021x)));

			g_object_unref (tmp_connection);
		}
	} else {
		nm_connection_remove_setting (connection, NM_TYPE_SETTING_802_1X);
		valid = TRUE;
	}

	return valid;
}

static void
ce_page_8021x_security_init (CEPage8021xSecurity *self)
{
}

static void
dispose (GObject *object)
{
	CEPage8021xSecurityPrivate *priv = CE_PAGE_8021X_SECURITY_GET_PRIVATE (object);

	if (priv->security) {
		wireless_security_unref (priv->security);
		priv->security = NULL;
	}

	G_OBJECT_CLASS (ce_page_8021x_security_parent_class)->dispose (object);
}

static void
ce_page_8021x_security_class_init (CEPage8021xSecurityClass *security_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (security_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (security_class);

	g_type_class_add_private (object_class, sizeof (CEPage8021xSecurityPrivate));

	/* virtual methods */
	object_class->dispose = dispose;

	parent_class->ce_page_validate_v = ce_page_validate_v;
}
