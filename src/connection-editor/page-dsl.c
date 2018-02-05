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
#include <linux/if.h>

#include "page-dsl.h"
#include "nm-connection-editor.h"
#include "nm-utils/nm-shared-utils.h"

G_DEFINE_TYPE (CEPageDsl, ce_page_dsl, CE_TYPE_PAGE)

#define CE_PAGE_DSL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_DSL, CEPageDslPrivate))

typedef struct {
	NMSettingPppoe *setting;

	GtkComboBoxText *parent;
	GtkEntry *interface;
	GtkLabel *interface_label;

	GtkEntry *username;
	GtkEntry *password;
	GtkEntry *service;
} CEPageDslPrivate;

/* The parent property is available in libnm 1.10, but since we only
 * require 1.8 at the moment, enable it only when detected at runtime.
 */
static gboolean parent_supported;

static void
find_unused_interface_name (NMClient *client, char *buf, gsize size)
{
	const GPtrArray *connections;
	NMConnection *con;
	const char *iface;
	gint64 num, ppp_num = 0;
	int i;

	connections = nm_client_get_connections (client);
	for (i = 0; i < connections->len; i++) {
		con = connections->pdata[i];

		if (!nm_connection_is_type (con, NM_SETTING_PPPOE_SETTING_NAME))
			continue;

		iface = nm_connection_get_interface_name (con);
		if (iface && g_str_has_prefix (iface, "ppp")) {
			num = _nm_utils_ascii_str_to_int64 (iface + 3, 10, 0, G_MAXUINT32, -1);
			if (num >= ppp_num)
				ppp_num = num + 1;
		}
	}

	g_snprintf (buf, size, "ppp%u", (unsigned) ppp_num);
}

static void
dsl_private_init (CEPageDsl *self)
{
	CEPageDslPrivate *priv = CE_PAGE_DSL_GET_PRIVATE (self);
	GtkBuilder *builder;

	builder = CE_PAGE (self)->builder;

	priv->parent = GTK_COMBO_BOX_TEXT (gtk_builder_get_object (builder, "dsl_parent"));
	priv->interface = GTK_ENTRY (gtk_builder_get_object (builder, "dsl_interface"));
	priv->interface_label = GTK_LABEL (gtk_builder_get_object (builder, "dsl_interface_label"));
	priv->username = GTK_ENTRY (gtk_builder_get_object (builder, "dsl_username"));
	priv->password = GTK_ENTRY (gtk_builder_get_object (builder, "dsl_password"));
	priv->service = GTK_ENTRY (gtk_builder_get_object (builder, "dsl_service"));
}

static void
populate_ui (CEPageDsl *self, NMConnection *connection)
{
	CEPageDslPrivate *priv = CE_PAGE_DSL_GET_PRIVATE (self);
	NMSettingPppoe *setting = priv->setting;
	gs_free char *parent = NULL;
	const char *str = NULL;
	char ifname[IFNAMSIZ];

	gtk_widget_set_visible (GTK_WIDGET (priv->interface), parent_supported);
	gtk_widget_set_visible (GTK_WIDGET (priv->interface_label), parent_supported);

	if (parent_supported)
		g_object_get (setting, "parent", &parent, NULL);

	if (parent) {
		ce_page_setup_device_combo (CE_PAGE (self), GTK_COMBO_BOX (priv->parent),
		                            G_TYPE_NONE, parent,
		                            NULL, NULL);

		str = nm_connection_get_interface_name (CE_PAGE (self)->connection);
		if (str)
			gtk_entry_set_text (priv->interface, str);
	} else {
		str = nm_connection_get_interface_name (CE_PAGE (self)->connection);
		ce_page_setup_device_combo (CE_PAGE (self), GTK_COMBO_BOX (priv->parent),
		                            G_TYPE_NONE, str,
		                            NULL, NULL);
	}

	if (parent_supported) {
		str = gtk_entry_get_text (priv->interface);
		if (!str || !str[0]) {
			find_unused_interface_name (CE_PAGE (self)->client, ifname, sizeof (ifname));
			gtk_entry_set_text (priv->interface, ifname);
		}
	}

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

	g_signal_connect (priv->parent, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->interface, "changed", G_CALLBACK (stuff_changed), self);
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
	                                 "/org/freedesktop/network-manager-applet/ce-page-dsl.ui",
	                                 "DslPage",
	                                 _("DSL/PPPoE")));
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
	const char *interface, *username, *password, *service;
	gs_free char *parent = NULL;
	NMSettingConnection *s_con;
	GtkWidget *entry;

	s_con = nm_connection_get_setting_connection (CE_PAGE (self)->connection);
	g_return_if_fail (s_con);

	if (parent_supported) {
		interface = gtk_entry_get_text (priv->interface);
		g_object_set (s_con,
		              NM_SETTING_CONNECTION_INTERFACE_NAME,
		              interface && interface[0] ? interface : NULL,
		              NULL);

		nm_connection_remove_setting (CE_PAGE (self)->connection, NM_TYPE_SETTING_WIRED);

		entry = gtk_bin_get_child (GTK_BIN (priv->parent));
		if (entry) {
			ce_page_device_entry_get (GTK_ENTRY (entry), ARPHRD_ETHER, TRUE,
			                          &parent, NULL, NULL, NULL);
		}
		g_object_set (priv->setting,
		              "parent", parent,
		              NULL);
	} else {
		entry = gtk_bin_get_child (GTK_BIN (priv->parent));
		if (entry) {
			ce_page_device_entry_get (GTK_ENTRY (entry), ARPHRD_ETHER, TRUE,
			                          &parent, NULL, NULL, NULL);
		}
		g_object_set (s_con,
		              NM_SETTING_CONNECTION_INTERFACE_NAME,
		              parent && parent[0] ? parent : NULL,
		              NULL);
	}

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
	gs_free char *parent = NULL;
	GtkWidget *entry;

	if (parent_supported) {
		entry = gtk_bin_get_child (GTK_BIN (priv->parent));
		if (entry) {
			ce_page_device_entry_get (GTK_ENTRY (entry), ARPHRD_ETHER, TRUE,
			                          &parent, NULL, NULL, NULL);
			if (!parent || !parent[0]) {
				g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC,
				                     _("missing parent interface"));
				return FALSE;
			}
		}
	}

	ui_to_setting (self);
	return nm_setting_verify (NM_SETTING (priv->setting), connection, error);
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

	parent_supported = !!nm_g_object_class_find_property_from_gtype (NM_TYPE_SETTING_PPPOE,
	                                                                 "parent");
}

void
dsl_connection_new (FUNC_TAG_PAGE_NEW_CONNECTION_IMPL,
                    GtkWindow *parent,
                    const char *detail,
                    gpointer detail_data,
                    NMConnection *connection,
                    NMClient *client,
                    PageNewConnectionResultFunc result_func,
                    gpointer user_data)
{
	NMSetting *setting;
	gs_unref_object NMConnection *connection_tmp = NULL;

	connection = _ensure_connection_other (connection, &connection_tmp);
	ce_page_complete_connection (connection,
	                             _("DSL connection %d"),
	                             NM_SETTING_PPPOE_SETTING_NAME,
	                             FALSE,
	                             client);
	nm_connection_add_setting (connection, nm_setting_pppoe_new ());
	nm_connection_add_setting (connection, nm_setting_wired_new ());
	setting = nm_setting_ppp_new ();
	/* Set default values for lcp-echo-failure and lcp-echo-interval */
	g_object_set (G_OBJECT (setting),
	              NM_SETTING_PPP_LCP_ECHO_FAILURE, 5,
	              NM_SETTING_PPP_LCP_ECHO_INTERVAL, 30,
	              NULL);
	nm_connection_add_setting (connection, setting);

	(*result_func) (FUNC_TAG_PAGE_NEW_CONNECTION_RESULT_CALL, connection, FALSE, NULL, user_data);
}
