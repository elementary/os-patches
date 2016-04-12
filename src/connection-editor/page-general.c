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
 * Copyright 2012 - 2014 Red Hat, Inc.
 */

#include "nm-default.h"

#include "page-general.h"

G_DEFINE_TYPE (CEPageGeneral, ce_page_general, CE_TYPE_PAGE)

#define CE_PAGE_GENERAL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_GENERAL, CEPageGeneralPrivate))

typedef struct {
	NMSettingConnection *setting;

	gboolean is_vpn;

	GDBusProxy *fw_proxy;
	GCancellable *cancellable;
	GtkComboBoxText *firewall_zone;
	char **zones;
	gboolean got_zones;

	GtkToggleButton *dependent_vpn_checkbox;
	GtkComboBox *dependent_vpn;
	GtkListStore *dependent_vpn_store;

	GtkWidget *autoconnect;
	GtkWidget *all_checkbutton;

	gboolean setup_finished;
} CEPageGeneralPrivate;

/* TRANSLATORS: Default zone set for firewall, when no zone is selected */
#define FIREWALL_ZONE_DEFAULT _("Default")
#define FIREWALL_ZONE_TOOLTIP_AVAILBALE _("The zone defines the trust level of the connection. Default is not a regular zone, selecting it results in the use of the default zone set in the firewall. Only usable if firewalld is active.")
#define FIREWALL_ZONE_TOOLTIP_UNAVAILBALE _("FirewallD is not running.")

enum {
	COL_ID,
	COL_UUID,
	N_COLUMNS
};

static void populate_firewall_zones_ui (CEPageGeneral *self);

static void
get_zones_cb (GDBusProxy *proxy, GAsyncResult *result, gpointer user_data)
{
	CEPageGeneral *self;
	CEPageGeneralPrivate *priv;
	GVariant *variant = NULL;
	GError *error = NULL;

	variant = g_dbus_proxy_call_finish (proxy, result, &error);
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_clear_error (&error);
		return;
	}

	self = CE_PAGE_GENERAL (user_data);
	priv = CE_PAGE_GENERAL_GET_PRIVATE (self);

	if (variant) {
		if (g_variant_is_of_type (variant, G_VARIANT_TYPE ("(as)")))
			g_variant_get (variant, "(^as)", &priv->zones);
		else {
			g_warning ("Failed to get zones from FirewallD: invalid reply type '%s'",
			           g_variant_get_type_string (variant));
		}
		g_variant_unref (variant);
	} else if (!g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN))
		g_warning ("Failed to get zones from FirewallD: %s", error->message);

	priv->got_zones = TRUE;
	if (priv->setup_finished)
		populate_firewall_zones_ui (self);

	g_clear_error (&error);
	g_clear_object (&priv->cancellable);
	g_clear_object (&priv->fw_proxy);
}

static void
on_fw_proxy_acquired (GObject *object, GAsyncResult *result, gpointer user_data)
{
	CEPageGeneral *self;
	CEPageGeneralPrivate *priv;
	GError *error = NULL;
	GDBusProxy *proxy;

	proxy = g_dbus_proxy_new_for_bus_finish (result, &error);
	if (!proxy) {
		g_warning ("Failed to get FirewallD proxy: %s", error->message);
		g_clear_error (&error);
		return;
	}

	self = CE_PAGE_GENERAL (user_data);
	priv = CE_PAGE_GENERAL_GET_PRIVATE (self);

	priv->fw_proxy = proxy;
	g_dbus_proxy_call (priv->fw_proxy,
	                   "getZones",
	                   NULL,
	                   G_DBUS_CALL_FLAGS_NONE,
	                   -1,
	                   priv->cancellable,
	                   (GAsyncReadyCallback) get_zones_cb,
	                   self);
}

static void
general_private_init (CEPageGeneral *self)
{
	CEPageGeneralPrivate *priv = CE_PAGE_GENERAL_GET_PRIVATE (self);
	GtkBuilder *builder;
	GtkWidget *vbox;
	GtkLabel *label;

	builder = CE_PAGE (self)->builder;

	/*-- Firewall zone --*/
	priv->firewall_zone = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new ());

	vbox = GTK_WIDGET (gtk_builder_get_object (builder, "firewall_zone_vbox"));
	gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (priv->firewall_zone));
	gtk_widget_show_all (GTK_WIDGET (priv->firewall_zone));

	/* Get zones from FirewallD */
	priv->cancellable = g_cancellable_new ();
	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
	                          G_DBUS_PROXY_FLAGS_NONE,
	                          NULL,
	                          "org.fedoraproject.FirewallD1",
	                          "/org/fedoraproject/FirewallD1",
	                          "org.fedoraproject.FirewallD1.zone",
	                          priv->cancellable,
	                          (GAsyncReadyCallback) on_fw_proxy_acquired,
	                          self);

	/* Set mnemonic widget for device Firewall zone label */
	label = GTK_LABEL (gtk_builder_get_object (builder, "firewall_zone_label"));
	gtk_label_set_mnemonic_widget (label, GTK_WIDGET (priv->firewall_zone));

	/*-- Dependent VPN connection --*/
	priv->dependent_vpn_checkbox = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "dependent_vpn_checkbox"));
	priv->dependent_vpn = GTK_COMBO_BOX (gtk_builder_get_object (builder, "dependent_vpn_combo"));
	priv->dependent_vpn_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "dependent_vpn_model"));

	priv->autoconnect = GTK_WIDGET (gtk_builder_get_object (builder, "connection_autoconnect"));
	priv->all_checkbutton = GTK_WIDGET (gtk_builder_get_object (builder, "system_checkbutton"));
}

static void
dispose (GObject *object)
{
	CEPageGeneralPrivate *priv = CE_PAGE_GENERAL_GET_PRIVATE (object);

	if (priv->cancellable) {
		g_cancellable_cancel (priv->cancellable);
		g_clear_object (&priv->cancellable);
	}
	g_clear_object (&priv->fw_proxy);

	g_clear_pointer (&priv->zones, g_strfreev);

	G_OBJECT_CLASS (ce_page_general_parent_class)->dispose (object);
}

static void
stuff_changed (GtkWidget *w, gpointer user_data)
{
	ce_page_changed (CE_PAGE (user_data));
}

static void
vpn_checkbox_toggled (GtkToggleButton *button, gpointer user_data)
{
	CEPageGeneralPrivate *priv = CE_PAGE_GENERAL_GET_PRIVATE (user_data);

	gtk_widget_set_sensitive (GTK_WIDGET (priv->dependent_vpn), gtk_toggle_button_get_active (priv->dependent_vpn_checkbox));
	ce_page_changed (CE_PAGE (user_data));
}

static void
populate_firewall_zones_ui (CEPageGeneral *self)
{
	CEPageGeneralPrivate *priv = CE_PAGE_GENERAL_GET_PRIVATE (self);
	NMSettingConnection *setting = priv->setting;
	const char *s_zone;
	char **zone_ptr;
	guint32 combo_idx = 0, idx;

	s_zone = nm_setting_connection_get_zone (setting);

	/* Always add "fake" 'Default' zone for default firewall settings */
	gtk_combo_box_text_append_text (priv->firewall_zone, FIREWALL_ZONE_DEFAULT);

	for (zone_ptr = priv->zones, idx = 0; zone_ptr && *zone_ptr; zone_ptr++, idx++) {
		gtk_combo_box_text_append_text (priv->firewall_zone, *zone_ptr);
		if (g_strcmp0 (s_zone, *zone_ptr) == 0)
			combo_idx = idx + 1;
	}

	if (s_zone && combo_idx == 0) {
		/* Unknown zone in connection setting - add it to combobox */
		gtk_combo_box_text_append_text (priv->firewall_zone, s_zone);
		combo_idx = idx + 1;
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (priv->firewall_zone), combo_idx);

	/* Zone tooltip and availability */
	if (priv->zones) {
		gtk_widget_set_tooltip_text (GTK_WIDGET (priv->firewall_zone), FIREWALL_ZONE_TOOLTIP_AVAILBALE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->firewall_zone), TRUE);
	} else {
		gtk_widget_set_tooltip_text (GTK_WIDGET (priv->firewall_zone), FIREWALL_ZONE_TOOLTIP_UNAVAILBALE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->firewall_zone), FALSE);
	}

	stuff_changed (NULL, self);
}

static void
populate_ui (CEPageGeneral *self)
{
	CEPageGeneralPrivate *priv = CE_PAGE_GENERAL_GET_PRIVATE (self);
	NMSettingConnection *setting = priv->setting;
	const char *vpn_uuid;
	guint32 combo_idx = 0, idx;
	const GPtrArray *con_list;
	int i;
	GtkTreeIter iter;
	gboolean global_connection = TRUE;

	/* Zones are filled when got them from firewalld */
	if (priv->got_zones)
		populate_firewall_zones_ui (self);

	/* Secondary UUID (VPN) */
	vpn_uuid = nm_setting_connection_get_secondary (setting, 0);
	con_list = nm_client_get_connections (CE_PAGE (self)->client);
	for (i = 0, idx = 0, combo_idx = 0; i < con_list->len; i++) {
		NMConnection *conn = con_list->pdata[i];
		const char *uuid = nm_connection_get_uuid (conn);
		const char *id = nm_connection_get_id (conn);

		if (!nm_connection_is_type (conn, NM_SETTING_VPN_SETTING_NAME))
			continue;

		gtk_list_store_append (priv->dependent_vpn_store, &iter);
		gtk_list_store_set (priv->dependent_vpn_store, &iter, COL_ID, id, COL_UUID, uuid, -1);
		if (g_strcmp0 (vpn_uuid, uuid) == 0)
			combo_idx = idx;
		idx++;
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (priv->dependent_vpn), combo_idx);

	/* We don't support multiple VPNs at the moment, so hide secondary
	 * stuff for VPN connections.  We'll revisit this later when we support
	 * multiple VPNs.
	 */
	if (priv->is_vpn) {
		gtk_widget_hide (GTK_WIDGET (priv->dependent_vpn_checkbox));
		gtk_widget_hide (GTK_WIDGET (priv->dependent_vpn));
	}

	/* 'Automatically connect to this network' checkbox */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->autoconnect),
	                              nm_setting_connection_get_autoconnect (priv->setting));

	/* VPN connections don't have a blanket "autoconnect" as that is too coarse
	 * a behavior, instead the user configures another connection to start the
	 * VPN on success.
	 */
	if (priv->is_vpn)
		gtk_widget_hide (priv->autoconnect);

	/* 'All users may connect to this network' checkbox */
	if (nm_setting_connection_get_num_permissions (priv->setting))
		global_connection = FALSE;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->all_checkbutton), global_connection);

	stuff_changed (NULL, self);
}

static void
finish_setup (CEPageGeneral *self, gpointer unused, GError *error, gpointer user_data)
{
	CEPageGeneralPrivate *priv = CE_PAGE_GENERAL_GET_PRIVATE (self);
	gboolean any_dependent_vpn;

	if (error)
		return;

	priv->setup_finished = TRUE;

	populate_ui (self);

	g_signal_connect (priv->firewall_zone, "changed", G_CALLBACK (stuff_changed), self);

	any_dependent_vpn = !!nm_setting_connection_get_num_secondaries (priv->setting);
	gtk_toggle_button_set_active (priv->dependent_vpn_checkbox, any_dependent_vpn);
	g_signal_connect (priv->dependent_vpn_checkbox, "toggled", G_CALLBACK (vpn_checkbox_toggled), self);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->dependent_vpn), any_dependent_vpn);
	g_signal_connect (priv->dependent_vpn, "changed", G_CALLBACK (stuff_changed), self);

	g_signal_connect (priv->autoconnect, "toggled", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->all_checkbutton, "toggled", G_CALLBACK (stuff_changed), self);
}

CEPage *
ce_page_general_new (NMConnectionEditor *editor,
                     NMConnection *connection,
                     GtkWindow *parent_window,
                     NMClient *client,
                     const char **out_secrets_setting_name,
                     GError **error)
{
	CEPageGeneral *self;
	CEPageGeneralPrivate *priv;

	self = CE_PAGE_GENERAL (ce_page_new (CE_TYPE_PAGE_GENERAL,
	                                     editor,
	                                     connection,
	                                     parent_window,
	                                     client,
	                                     UIDIR "/ce-page-general.ui",
	                                     "GeneralPage",
	                                     _("General")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC,
		                     _("Could not load General user interface."));
		return NULL;
	}

	general_private_init (self);
	priv = CE_PAGE_GENERAL_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_connection (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_CONNECTION (nm_setting_connection_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}

	priv->is_vpn = nm_connection_is_type (connection, NM_SETTING_VPN_SETTING_NAME);

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return CE_PAGE (self);
}

static void
ui_to_setting (CEPageGeneral *self)
{
	CEPageGeneralPrivate *priv = CE_PAGE_GENERAL_GET_PRIVATE (self);
	char *uuid = NULL;
	GtkTreeIter iter;
	gboolean autoconnect = FALSE, everyone = FALSE;

	/* We can't take and save zone until the combo was properly initialized. Zones
	 * are received from FirewallD asynchronously; got_zones indicates we are ready.
	 */
	if (priv->got_zones) {
		char *zone;

		zone = gtk_combo_box_text_get_active_text (priv->firewall_zone);
		g_object_set (priv->setting, NM_SETTING_CONNECTION_ZONE,
		              (g_strcmp0 (zone, FIREWALL_ZONE_DEFAULT) != 0) ? zone : NULL,
		              NULL);
		g_free (zone);
	}

	if (   gtk_toggle_button_get_active (priv->dependent_vpn_checkbox)
	    && gtk_combo_box_get_active_iter (priv->dependent_vpn, &iter))
		gtk_tree_model_get (GTK_TREE_MODEL (priv->dependent_vpn_store), &iter,
		                                    COL_UUID, &uuid, -1);

	g_object_set (G_OBJECT (priv->setting), NM_SETTING_CONNECTION_SECONDARIES, NULL, NULL);
	if (uuid)
		nm_setting_connection_add_secondary (priv->setting, uuid);
	g_free (uuid);

	autoconnect = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->autoconnect));
	g_object_set (G_OBJECT (priv->setting), NM_SETTING_CONNECTION_AUTOCONNECT, autoconnect, NULL);

	/* Handle visibility */
	everyone = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->all_checkbutton));
	g_object_set (G_OBJECT (priv->setting), NM_SETTING_CONNECTION_PERMISSIONS, NULL, NULL);
	if (everyone == FALSE) {
		/* Only visible to this user */
		nm_setting_connection_add_permission (priv->setting, "user", g_get_user_name (), NULL);
	}
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageGeneral *self = CE_PAGE_GENERAL (page);
	CEPageGeneralPrivate *priv = CE_PAGE_GENERAL_GET_PRIVATE (self);

	ui_to_setting (self);
	return nm_setting_verify (NM_SETTING (priv->setting), NULL, error);
}

static void
ce_page_general_init (CEPageGeneral *self)
{
}

static void
ce_page_general_class_init (CEPageGeneralClass *connection_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (connection_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (connection_class);

	g_type_class_add_private (object_class, sizeof (CEPageGeneralPrivate));

	/* virtual methods */
	object_class->dispose = dispose;

	parent_class->ce_page_validate_v = ce_page_validate_v;
}

