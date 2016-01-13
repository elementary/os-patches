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
 * (C) Copyright 2008 - 2012 Red Hat, Inc.
 */

#include "config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <nm-setting-connection.h>
#include <nm-setting-pppoe.h>
#include <nm-setting-ppp.h>
#include <nm-setting-wired.h>

#include "page-dsl.h"
#include "nm-connection-editor.h"

G_DEFINE_TYPE (CEPageDsl, ce_page_dsl, CE_TYPE_PAGE)

#define CE_PAGE_DSL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_DSL, CEPageDslPrivate))

typedef struct {
	NMSettingPPPOE *setting;

	GtkEntry *username;
	GtkEntry *password;
	GtkEntry *service;
} CEPageDslPrivate;

static void
dsl_private_init (CEPageDsl *self)
{
	CEPageDslPrivate *priv = CE_PAGE_DSL_GET_PRIVATE (self);
	GtkBuilder *builder;

	builder = CE_PAGE (self)->builder;

	priv->username = GTK_ENTRY (gtk_builder_get_object (builder, "dsl_username"));
	priv->password = GTK_ENTRY (gtk_builder_get_object (builder, "dsl_password"));
	priv->service = GTK_ENTRY (gtk_builder_get_object (builder, "dsl_service"));
}

static void
populate_ui (CEPageDsl *self, NMConnection *connection)
{
	CEPageDslPrivate *priv = CE_PAGE_DSL_GET_PRIVATE (self);
	NMSettingPPPOE *setting = priv->setting;
	const char *str;

	str = nm_setting_pppoe_get_username (setting);
	if (str)
		gtk_entry_set_text (priv->username, str);

	/* Grab password from keyring if possible */
	str = nm_setting_pppoe_get_password (setting);
	if (str)
		gtk_entry_set_text (priv->password, str);

	str = nm_setting_pppoe_get_service (setting);
	if (str)
		gtk_entry_set_text (priv->service, str);
}

static void
stuff_changed (GtkEditable *editable, gpointer user_data)
{
	ce_page_changed (CE_PAGE (user_data));
}

static void
show_password (GtkToggleButton *button, gpointer user_data)
{
	CEPageDslPrivate *priv = CE_PAGE_DSL_GET_PRIVATE (user_data);

	gtk_entry_set_visibility (priv->password, gtk_toggle_button_get_active (button));
}

static void
finish_setup (CEPageDsl *self, gpointer unused, GError *error, gpointer user_data)
{
	CEPage *parent = CE_PAGE (self);
	CEPageDslPrivate *priv = CE_PAGE_DSL_GET_PRIVATE (self);

	if (error)
		return;

	populate_ui (self, parent->connection);

	g_signal_connect (priv->username, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->password, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->service, "changed", G_CALLBACK (stuff_changed), self);

	g_signal_connect (GTK_WIDGET (gtk_builder_get_object (parent->builder, "dsl_show_password")), "toggled",
					  G_CALLBACK (show_password), self);
}

CEPage *
ce_page_dsl_new (NMConnectionEditor *editor,
                 NMConnection *connection,
                 GtkWindow *parent_window,
                 NMClient *client,
                 NMRemoteSettings *settings,
                 const char **out_secrets_setting_name,
                 GError **error)
{
	CEPageDsl *self;
	CEPageDslPrivate *priv;

	self = CE_PAGE_DSL (ce_page_new (CE_TYPE_PAGE_DSL,
	                                 editor,
	                                 connection,
	                                 parent_window,
	                                 client,
	                                 settings,
	                                 UIDIR "/ce-page-dsl.ui",
	                                 "DslPage",
	                                 _("DSL")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load DSL user interface."));
		return NULL;
	}

	dsl_private_init (self);
	priv = CE_PAGE_DSL_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_pppoe (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_PPPOE (nm_setting_pppoe_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	*out_secrets_setting_name = NM_SETTING_PPPOE_SETTING_NAME;

	return CE_PAGE (self);
}

static void
ui_to_setting (CEPageDsl *self)
{
	CEPageDslPrivate *priv = CE_PAGE_DSL_GET_PRIVATE (self);
	const char *username;
	const char *password;
	const char *service;

	username = gtk_entry_get_text (priv->username);
	if (username && strlen (username) < 1)
		username = NULL;

	password = gtk_entry_get_text (priv->password);
	if (password && strlen (password) < 1)
		password = NULL;

	service = gtk_entry_get_text (priv->service);
	if (service && strlen (service) < 1)
		service = NULL;

	g_object_set (priv->setting,
	              NM_SETTING_PPPOE_USERNAME, username,
	              NM_SETTING_PPPOE_PASSWORD, password,
	              NM_SETTING_PPPOE_SERVICE, service,
	              NULL);
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageDsl *self = CE_PAGE_DSL (page);
	CEPageDslPrivate *priv = CE_PAGE_DSL_GET_PRIVATE (self);
	GSList *foo;
	gboolean valid;

	ui_to_setting (self);

	foo = g_slist_append (NULL, nm_connection_get_setting_ppp (connection));
	valid = nm_setting_verify (NM_SETTING (priv->setting), foo, error);
	g_slist_free (foo);

	return valid;
}

static void
ce_page_dsl_init (CEPageDsl *self)
{
}

static void
ce_page_dsl_class_init (CEPageDslClass *dsl_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (dsl_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (dsl_class);

	g_type_class_add_private (object_class, sizeof (CEPageDslPrivate));

	/* virtual methods */
	parent_class->ce_page_validate_v = ce_page_validate_v;
}


void
dsl_connection_new (GtkWindow *parent,
                    const char *detail,
                    NMRemoteSettings *settings,
                    PageNewConnectionResultFunc result_func,
                    gpointer user_data)
{
	NMConnection *connection;
	NMSetting *setting;

	connection = ce_page_new_connection (_("DSL connection %d"),
	                                     NM_SETTING_PPPOE_SETTING_NAME,
	                                     FALSE,
	                                     settings,
	                                     user_data);
	nm_connection_add_setting (connection, nm_setting_pppoe_new ());
	nm_connection_add_setting (connection, nm_setting_wired_new ());
	setting = nm_setting_ppp_new ();
	/* Set default values for lcp-echo-failure and lcp-echo-interval */
	g_object_set (G_OBJECT (setting),
	              NM_SETTING_PPP_LCP_ECHO_FAILURE, 5,
	              NM_SETTING_PPP_LCP_ECHO_INTERVAL, 30,
	              NULL);
	nm_connection_add_setting (connection, setting);

	(*result_func) (connection, FALSE, NULL, user_data);
}


