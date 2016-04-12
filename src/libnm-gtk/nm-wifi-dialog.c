/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Applet -- allow user control over networking
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
 * (C) Copyright 2007 - 2012 Red Hat, Inc.
 */

#include "nm-default.h"

#include <string.h>
#include <netinet/ether.h>

#include <nm-client.h>
#include <nm-utils.h>
#include <nm-device-wifi.h>
#include <nm-setting-connection.h>
#include <nm-setting-wireless.h>
#include <nm-setting-ip4-config.h>

#include "nm-wifi-dialog.h"
#include "wireless-security.h"
#include "nm-ui-utils.h"
#include "eap-method.h"

G_DEFINE_TYPE (NMAWifiDialog, nma_wifi_dialog, GTK_TYPE_DIALOG)

#define NMA_WIFI_DIALOG_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                        NMA_TYPE_WIFI_DIALOG, \
                                        NMAWifiDialogPrivate))

typedef struct {
	NMAWifiDialog *self;
	NMConnection *connection;
	gboolean canceled;
} GetSecretsInfo;

typedef struct {
	NMClient *client;
	NMRemoteSettings *settings;

	GtkBuilder *builder;

	NMConnection *connection;
	NMDevice *device;
	NMAccessPoint *ap;
	guint operation;

	GtkTreeModel *device_model;
	GtkTreeModel *connection_model;
	GtkSizeGroup *group;
	GtkWidget *sec_combo;
	GtkWidget *ok_response_button;

	gboolean network_name_focus;

	gboolean secrets_only;

	guint revalidate_id;

	GetSecretsInfo *secrets_info;

	gboolean disposed;
} NMAWifiDialogPrivate;

enum {
	OP_NONE = 0,
	OP_CREATE_ADHOC,
	OP_CONNECT_HIDDEN,
};

#define D_NAME_COLUMN		0
#define D_DEV_COLUMN		1

#define S_NAME_COLUMN		0
#define S_SEC_COLUMN		1

#define C_NAME_COLUMN		0
#define C_CON_COLUMN		1
#define C_SEP_COLUMN		2
#define C_NEW_COLUMN		3

static gboolean security_combo_init (NMAWifiDialog *self, gboolean secrets_only);
static void ssid_entry_changed (GtkWidget *entry, gpointer user_data);

void
nma_wifi_dialog_set_nag_ignored (NMAWifiDialog *self, gboolean ignored)
{
}

gboolean
nma_wifi_dialog_get_nag_ignored (NMAWifiDialog *self)
{
	return TRUE;
}

static void
size_group_clear (GtkSizeGroup *group)
{
	GSList *iter;

	g_return_if_fail (group != NULL);

	iter = gtk_size_group_get_widgets (group);
	while (iter) {
		gtk_size_group_remove_widget (group, GTK_WIDGET (iter->data));
		iter = gtk_size_group_get_widgets (group);
	}
}

static void
_set_response_sensitive (NMAWifiDialog *self,
                         int response_id,
                         gboolean is_sensitive)
{
	switch (response_id) {
	case GTK_RESPONSE_CANCEL:
	case GTK_RESPONSE_OK:
		gtk_dialog_set_response_sensitive (GTK_DIALOG (self), response_id, is_sensitive);

		if (response_id == GTK_RESPONSE_OK) {
			NMAWifiDialogPrivate *priv = NMA_WIFI_DIALOG_GET_PRIVATE (self);

			if (priv->ok_response_button) {
				gtk_widget_set_tooltip_text (priv->ok_response_button,
				                             is_sensitive
				                                 ? _("Click to connect")
				                                 : _("Either a password is missing or the connection is invalid. In the latter case, you have to edit the connection with nm-connection-editor first"));
			}
		}
		break;
	default:
		g_return_if_reached ();
	}
}

static void
size_group_add_permanent (GtkSizeGroup *group,
                          GtkBuilder *builder)
{
	GtkWidget *widget;

	g_return_if_fail (group != NULL);
	g_return_if_fail (builder != NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "network_name_label"));
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "security_combo_label"));
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "device_label"));
	gtk_size_group_add_widget (group, widget);
}

static void
security_combo_changed (GtkWidget *combo,
                        gpointer user_data)
{
	NMAWifiDialog *self = NMA_WIFI_DIALOG (user_data);
	NMAWifiDialogPrivate *priv = NMA_WIFI_DIALOG_GET_PRIVATE (self);
	GtkWidget *vbox, *sec_widget, *def_widget;
	GList *elt, *children;
	GtkTreeIter iter;
	GtkTreeModel *model;
	WirelessSecurity *sec = NULL;

	vbox = GTK_WIDGET (gtk_builder_get_object (priv->builder, "security_vbox"));
	g_assert (vbox);

	size_group_clear (priv->group);

	/* Remove any previous wireless security widgets */
	children = gtk_container_get_children (GTK_CONTAINER (vbox));
	for (elt = children; elt; elt = g_list_next (elt))
		gtk_container_remove (GTK_CONTAINER (vbox), GTK_WIDGET (elt->data));
	g_list_free (children);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter)) {
		g_warning ("%s: no active security combo box item.", __func__);
		return;
	}

	gtk_tree_model_get (model, &iter, S_SEC_COLUMN, &sec, -1);
	if (!sec) {
		/* Revalidate dialog if the user picked "None" so the OK button
		 * gets enabled if there's already a valid SSID.
		 */
		ssid_entry_changed (NULL, self);
		return;
	}

	sec_widget = wireless_security_get_widget (sec);
	g_assert (sec_widget);
	gtk_widget_unparent (sec_widget);

	size_group_add_permanent (priv->group, priv->builder);
	wireless_security_add_to_size_group (sec, priv->group);

	gtk_container_add (GTK_CONTAINER (vbox), sec_widget);

	/* Re-validate */
	wireless_security_changed_cb (NULL, sec);

	/* Set focus to the security method's default widget, but only if the
	 * network name entry should not be focused.
	 */
	if (!priv->network_name_focus && sec->default_field) {
		def_widget = GTK_WIDGET (gtk_builder_get_object (sec->builder, sec->default_field));
		if (def_widget)
			gtk_widget_grab_focus (def_widget);
	}

	wireless_security_unref (sec);
}

static void
security_combo_changed_manually (GtkWidget *combo,
                                 gpointer user_data)
{
	NMAWifiDialog *self = NMA_WIFI_DIALOG (user_data);
	NMAWifiDialogPrivate *priv = NMA_WIFI_DIALOG_GET_PRIVATE (self);

	/* Flag that the combo was changed manually to allow focus to move
	 * to the security method's default widget instead of the network name.
	 */
	priv->network_name_focus = FALSE;
	security_combo_changed (combo, user_data);
}

static GByteArray *
validate_dialog_ssid (NMAWifiDialog *self)
{
	NMAWifiDialogPrivate *priv = NMA_WIFI_DIALOG_GET_PRIVATE (self);
	GtkWidget *widget;
	const char *ssid;
	guint32 ssid_len;
	GByteArray *ssid_ba;

	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "network_name_entry"));

	ssid = gtk_entry_get_text (GTK_ENTRY (widget));
	
	if (!ssid || strlen (ssid) == 0 || strlen (ssid) > 32)
		return NULL;

	ssid_len = strlen (ssid);
	ssid_ba = g_byte_array_sized_new (ssid_len);
	g_byte_array_append (ssid_ba, (unsigned char *) ssid, ssid_len);
	return ssid_ba;
}

static void
stuff_changed_cb (WirelessSecurity *sec, gpointer user_data)
{
	NMAWifiDialog *self = NMA_WIFI_DIALOG (user_data);
	NMAWifiDialogPrivate *priv = NMA_WIFI_DIALOG_GET_PRIVATE (self);
	GByteArray *ssid = NULL;
	gboolean free_ssid = TRUE;
	gboolean valid = FALSE;

	if (priv->connection) {
		NMSettingWireless *s_wireless;
		s_wireless = nm_connection_get_setting_wireless (priv->connection);
		g_assert (s_wireless);
		ssid = (GByteArray *) nm_setting_wireless_get_ssid (s_wireless);
		free_ssid = FALSE;
	} else {
		ssid = validate_dialog_ssid (self);
	}

	if (ssid) {
		valid = wireless_security_validate (sec, NULL);
		if (free_ssid)
			g_byte_array_free (ssid, TRUE);
	}

	/* But if there's an in-progress secrets call (which might require authorization)
	 * then we don't want to enable the OK button because we don't have all the
	 * connection details yet.
	 */
	if (priv->secrets_info)
		valid = FALSE;

	_set_response_sensitive (self, GTK_RESPONSE_OK, valid);
}

static void
ssid_entry_changed (GtkWidget *entry, gpointer user_data)
{
	NMAWifiDialog *self = NMA_WIFI_DIALOG (user_data);
	NMAWifiDialogPrivate *priv = NMA_WIFI_DIALOG_GET_PRIVATE (self);
	GtkTreeIter iter;
	WirelessSecurity *sec = NULL;
	GtkTreeModel *model;
	gboolean valid = FALSE;
	GByteArray *ssid;

	/* If the network name entry was touched at all, allow focus to go to
	 * the default widget of the security method now.
	 */
	priv->network_name_focus = FALSE;

	ssid = validate_dialog_ssid (self);
	if (!ssid)
		goto out;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->sec_combo));
	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->sec_combo), &iter))
		gtk_tree_model_get (model, &iter, S_SEC_COLUMN, &sec, -1);

	if (sec) {
		valid = wireless_security_validate (sec, NULL);
		wireless_security_unref (sec);
	} else {
		valid = TRUE;
	}

out:
	/* But if there's an in-progress secrets call (which might require authorization)
	 * then we don't want to enable the OK button because we don't have all the
	 * connection details yet.
	 */
	if (priv->secrets_info)
		valid = FALSE;

	_set_response_sensitive (self, GTK_RESPONSE_OK, valid);
}

static void
connection_combo_changed (GtkWidget *combo,
                          gpointer user_data)
{
	NMAWifiDialog *self = NMA_WIFI_DIALOG (user_data);
	NMAWifiDialogPrivate *priv = NMA_WIFI_DIALOG_GET_PRIVATE (self);
	GtkTreeIter iter;
	GtkTreeModel *model;
	gboolean is_new = FALSE;
	NMSettingWireless *s_wireless;
	char *utf8_ssid;
	GtkWidget *widget;

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter)) {
		g_warning ("%s: no active connection combo box item.", __func__);
		return;
	}

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

	if (priv->connection)
		g_object_unref (priv->connection);

	gtk_tree_model_get (model, &iter,
	                    C_CON_COLUMN, &priv->connection,
	                    C_NEW_COLUMN, &is_new, -1);

	if (priv->connection)
		eap_method_ca_cert_ignore_load (priv->connection);

	if (!security_combo_init (self, priv->secrets_only)) {
		g_warning ("Couldn't change Wi-Fi security combo box.");
		return;
	}
	security_combo_changed (priv->sec_combo, self);

	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "network_name_entry"));
	if (priv->connection) {
		const GByteArray *ssid;

		s_wireless = nm_connection_get_setting_wireless (priv->connection);
		ssid = nm_setting_wireless_get_ssid (s_wireless);
		utf8_ssid = nm_utils_ssid_to_utf8 (ssid);
		gtk_entry_set_text (GTK_ENTRY (widget), utf8_ssid);
		g_free (utf8_ssid);
	} else {
		gtk_entry_set_text (GTK_ENTRY (widget), "");
	}

	gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (priv->builder, "network_name_entry")), is_new);
	gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (priv->builder, "network_name_label")), is_new);
	gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (priv->builder, "security_combo")), is_new);
	gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (priv->builder, "security_combo_label")), is_new);
	gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (priv->builder, "security_vbox")), is_new);
}

static gboolean
connection_combo_separator_cb (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gboolean is_separator = FALSE;

	gtk_tree_model_get (model, iter, C_SEP_COLUMN, &is_separator, -1);
	return is_separator;
}

static gint
alphabetize_connections (NMConnection *a, NMConnection *b)
{
	NMSettingConnection *asc, *bsc;

	asc = nm_connection_get_setting_connection (a);
	bsc = nm_connection_get_setting_connection (b);

	return strcmp (nm_setting_connection_get_id (asc),
		       nm_setting_connection_get_id (bsc));
}

static gboolean
connection_combo_init (NMAWifiDialog *self, NMConnection *connection)
{
	NMAWifiDialogPrivate *priv = NMA_WIFI_DIALOG_GET_PRIVATE (self);
	GtkListStore *store;
	int num_added = 0;
	GtkTreeIter tree_iter;
	GtkWidget *widget;
	NMSettingConnection *s_con;
	GtkCellRenderer *renderer;
	const char *id;

	g_return_val_if_fail (priv->connection == NULL, FALSE);

	/* Clear any old model */
	if (priv->connection_model)
		g_object_unref (priv->connection_model);

	/* New model */
	store = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_OBJECT, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
	priv->connection_model = GTK_TREE_MODEL (store);

	if (connection) {
		s_con = nm_connection_get_setting_connection (connection);
		g_assert (s_con);
		id = nm_setting_connection_get_id (s_con);
		if (id == NULL) {
			/* New connections which will be completed by NM won't have an ID
			 * yet, but that doesn't matter because we don't show the connection
			 * combo anyway when there's a predefined connection.
			 */
			id = "blahblah";
		}

		gtk_list_store_append (store, &tree_iter);
		gtk_list_store_set (store, &tree_iter,
		                    C_NAME_COLUMN, id,
		                    C_CON_COLUMN, connection, -1);
	} else {
		GSList *connections, *iter, *to_add = NULL;

		gtk_list_store_append (store, &tree_iter);
		gtk_list_store_set (store, &tree_iter,
		                    C_NAME_COLUMN, _("New..."),
		                    C_NEW_COLUMN, TRUE, -1);

		gtk_list_store_append (store, &tree_iter);
		gtk_list_store_set (store, &tree_iter, C_SEP_COLUMN, TRUE, -1);

		connections = nm_remote_settings_list_connections (priv->settings);
		for (iter = connections; iter; iter = g_slist_next (iter)) {
			NMConnection *candidate = NM_CONNECTION (iter->data);
			NMSettingWireless *s_wireless;
			const char *connection_type;
			const char *mode;
			const GByteArray *setting_mac;

			s_con = nm_connection_get_setting_connection (candidate);
			connection_type = s_con ? nm_setting_connection_get_connection_type (s_con) : NULL;
			if (!connection_type)
				continue;

			if (strcmp (connection_type, NM_SETTING_WIRELESS_SETTING_NAME))
				continue;

			s_wireless = nm_connection_get_setting_wireless (candidate);
			if (!s_wireless)
				continue;

			/* If creating a new Ad-Hoc network, only show shared network connections */
			if (priv->operation == OP_CREATE_ADHOC) {
				NMSettingIP4Config *s_ip4;
				const char *method = NULL;

				s_ip4 = nm_connection_get_setting_ip4_config (candidate);
				if (s_ip4)
					method = nm_setting_ip4_config_get_method (s_ip4);

				if (!s_ip4 || strcmp (method, "shared"))
					continue;

				/* Ignore non-Ad-Hoc connections too */
				mode = nm_setting_wireless_get_mode (s_wireless);
				if (!mode || (strcmp (mode, "adhoc") && strcmp (mode, "ap")))
					continue;
			}

			/* Ignore connections that don't apply to the selected device */
			setting_mac = nm_setting_wireless_get_mac_address (s_wireless);
			if (setting_mac) {
				const char *hw_addr;

				hw_addr = nm_device_wifi_get_hw_address (NM_DEVICE_WIFI (priv->device));
				if (hw_addr) {
					struct ether_addr *ether;

					ether = ether_aton (hw_addr);
					if (ether && memcmp (setting_mac->data, ether->ether_addr_octet, ETH_ALEN))
						continue;
				}
			}

			to_add = g_slist_append (to_add, candidate);
		}
		g_slist_free (connections);

		/* Alphabetize the list then add the connections */
		to_add = g_slist_sort (to_add, (GCompareFunc) alphabetize_connections);
		for (iter = to_add; iter; iter = g_slist_next (iter)) {
			NMConnection *candidate = NM_CONNECTION (iter->data);

			s_con = nm_connection_get_setting_connection (candidate);
			gtk_list_store_append (store, &tree_iter);
			gtk_list_store_set (store, &tree_iter,
			                    C_NAME_COLUMN, nm_setting_connection_get_id (s_con),
			                    C_CON_COLUMN, candidate, -1);
			num_added++;
		}
		g_slist_free (to_add);
	}

	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "connection_combo"));

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (widget));
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), renderer, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (widget), renderer,
	                               "text", C_NAME_COLUMN);
	gtk_combo_box_set_wrap_width (GTK_COMBO_BOX (widget), 1);

	gtk_combo_box_set_model (GTK_COMBO_BOX (widget), priv->connection_model);

	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (widget),
	                                      connection_combo_separator_cb,
	                                      NULL,
	                                      NULL);

	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	g_signal_connect (G_OBJECT (widget), "changed",
	                  G_CALLBACK (connection_combo_changed), self);
	if (connection || !num_added) {
		gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (priv->builder, "connection_label")));
		gtk_widget_hide (widget);
	}
	if (gtk_tree_model_get_iter_first (priv->connection_model, &tree_iter))
		gtk_tree_model_get (priv->connection_model, &tree_iter, C_CON_COLUMN, &priv->connection, -1);

	return TRUE;
}

static void
device_combo_changed (GtkWidget *combo,
                      gpointer user_data)
{
	NMAWifiDialog *self = NMA_WIFI_DIALOG (user_data);
	NMAWifiDialogPrivate *priv = NMA_WIFI_DIALOG_GET_PRIVATE (self);
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter)) {
		g_warning ("%s: no active device combo box item.", __func__);
		return;
	}
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

	g_object_unref (priv->device);
	gtk_tree_model_get (model, &iter, D_DEV_COLUMN, &priv->device, -1);

	if (!connection_combo_init (self, NULL)) {
		g_warning ("Couldn't change connection combo box.");
		return;
	}

	if (!security_combo_init (self, priv->secrets_only)) {
		g_warning ("Couldn't change Wi-Fi security combo box.");
		return;
	}

	security_combo_changed (priv->sec_combo, self);
}

static void
add_device_to_model (GtkListStore *model, NMDevice *device)
{
	GtkTreeIter iter;
	const char *desc;

	desc = nma_utils_get_device_description (device);
	gtk_list_store_append (model, &iter);
	gtk_list_store_set (model, &iter, D_NAME_COLUMN, desc, D_DEV_COLUMN, device, -1);
}

static gboolean
can_use_device (NMDevice *device)
{
	/* Ignore unsupported devices */
	if (!(nm_device_get_capabilities (device) & NM_DEVICE_CAP_NM_SUPPORTED))
		return FALSE;

	if (!NM_IS_DEVICE_WIFI (device))
		return FALSE;

	if (nm_device_get_state (device) < NM_DEVICE_STATE_DISCONNECTED)
		return FALSE;

	return TRUE;
}

static gboolean
device_combo_init (NMAWifiDialog *self, NMDevice *device)
{
	NMAWifiDialogPrivate *priv = NMA_WIFI_DIALOG_GET_PRIVATE (self);
	const GPtrArray *devices;
	GtkListStore *store;
	int i, num_added = 0;

	g_return_val_if_fail (priv->device == NULL, FALSE);

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_OBJECT);
	priv->device_model = GTK_TREE_MODEL (store);

	if (device) {
		if (!can_use_device (device))
			return FALSE;
		add_device_to_model (store, device);
		num_added++;
	} else {
		devices = nm_client_get_devices (priv->client);
		if (!devices)
			return FALSE;

		for (i = 0; devices && (i < devices->len); i++) {
			device = NM_DEVICE (g_ptr_array_index (devices, i));
			if (can_use_device (device)) {
				add_device_to_model (store, device);
				num_added++;
			}
		}
	}

	if (num_added > 0) {
		GtkWidget *widget;
		GtkTreeIter iter;

		widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "device_combo"));
		gtk_combo_box_set_model (GTK_COMBO_BOX (widget), priv->device_model);
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
		g_signal_connect (G_OBJECT (widget), "changed",
		                  G_CALLBACK (device_combo_changed), self);
		if (num_added == 1) {
			gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (priv->builder, "device_label")));
			gtk_widget_hide (widget);
		}
		if (gtk_tree_model_get_iter_first (priv->device_model, &iter))
			gtk_tree_model_get (priv->device_model, &iter, D_DEV_COLUMN, &priv->device, -1);
	}

	return num_added > 0 ? TRUE : FALSE;
}

static gboolean
find_proto (NMSettingWirelessSecurity *sec, const char *item)
{
	guint32 i;

	for (i = 0; i < nm_setting_wireless_security_get_num_protos (sec); i++) {
		if (!strcmp (item, nm_setting_wireless_security_get_proto (sec, i)))
			return TRUE;
	}
	return FALSE;
}

static NMUtilsSecurityType
get_default_type_for_security (NMSettingWirelessSecurity *sec,
                               gboolean have_ap,
                               guint32 ap_flags,
                               guint32 dev_caps)
{
	const char *key_mgmt, *auth_alg;

	g_return_val_if_fail (sec != NULL, NMU_SEC_NONE);

	key_mgmt = nm_setting_wireless_security_get_key_mgmt (sec);
	auth_alg = nm_setting_wireless_security_get_auth_alg (sec);

	/* No IEEE 802.1x */
	if (!strcmp (key_mgmt, "none"))
		return NMU_SEC_STATIC_WEP;

	if (   !strcmp (key_mgmt, "ieee8021x")
	    && (!have_ap || (ap_flags & NM_802_11_AP_FLAGS_PRIVACY))) {
		if (auth_alg && !strcmp (auth_alg, "leap"))
			return NMU_SEC_LEAP;
		return NMU_SEC_DYNAMIC_WEP;
	}

	if (   !strcmp (key_mgmt, "wpa-none")
	    || !strcmp (key_mgmt, "wpa-psk")) {
		if (!have_ap || (ap_flags & NM_802_11_AP_FLAGS_PRIVACY)) {
			if (find_proto (sec, "rsn"))
				return NMU_SEC_WPA2_PSK;
			else if (find_proto (sec, "wpa"))
				return NMU_SEC_WPA_PSK;
			else
				return NMU_SEC_WPA_PSK;
		}
	}

	if (   !strcmp (key_mgmt, "wpa-eap")
	    && (!have_ap || (ap_flags & NM_802_11_AP_FLAGS_PRIVACY))) {
			if (find_proto (sec, "rsn"))
				return NMU_SEC_WPA2_ENTERPRISE;
			else if (find_proto (sec, "wpa"))
				return NMU_SEC_WPA_ENTERPRISE;
			else
				return NMU_SEC_WPA_ENTERPRISE;
	}

	return NMU_SEC_INVALID;
}

static void
add_security_item (NMAWifiDialog *self,
                   WirelessSecurity *sec,
                   GtkListStore *model,
                   GtkTreeIter *iter,
                   const char *text)
{
	wireless_security_set_changed_notify (sec, stuff_changed_cb, self);
	gtk_list_store_append (model, iter);
	gtk_list_store_set (model, iter, S_NAME_COLUMN, text, S_SEC_COLUMN, sec, -1);
	wireless_security_unref (sec);
}

static void
get_secrets_cb (NMRemoteConnection *connection,
                GHashTable *secrets,
                GError *error,
                gpointer user_data)
{
	GetSecretsInfo *info = user_data;
	NMAWifiDialogPrivate *priv;
	GHashTableIter hash_iter;
	gpointer key, value;
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (info->canceled)
		goto out;

	priv = NMA_WIFI_DIALOG_GET_PRIVATE (info->self);
	if (priv->secrets_info == info) {
		priv->secrets_info = NULL;

		/* Buttons should only be re-enabled if this secrets response is the
		 * in-progress one.
		 */
		_set_response_sensitive (info->self, GTK_RESPONSE_CANCEL, TRUE);
		_set_response_sensitive (info->self, GTK_RESPONSE_OK, TRUE);
	}

	if (error) {
		g_warning ("%s: error getting connection secrets: (%d) %s",
		           __func__,
		           error ? error->code : -1,
		           error && error->message ? error->message : "(unknown)");
		goto out;
	}

	/* User might have changed the connection while the secrets call was in
	 * progress, so don't try to update the wrong connection with the secrets
	 * we just received.
	 */
	if (   (priv->connection != info->connection)
	    || !secrets
	    || !g_hash_table_size (secrets))
		goto out;

	/* Try to update the connection's secrets; log errors but we don't care */
	g_hash_table_iter_init (&hash_iter, secrets);
	while (g_hash_table_iter_next (&hash_iter, &key, &value)) {
		GError *update_error = NULL;
		const char *setting_name = key;
		GHashTable *setting_hash = value;

		if (!nm_connection_update_secrets (priv->connection,
		                                   setting_name,
		                                   setting_hash,
		                                   &update_error)) {
			g_warning ("%s: error updating connection secrets: (%d) %s",
			           __func__,
			           update_error ? update_error->code : -1,
			           update_error && update_error->message ? update_error->message : "(unknown)");
			g_clear_error (&update_error);
		}
	}

	/* Update each security method's UI elements with the new secrets */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->sec_combo));
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			WirelessSecurity *sec = NULL;

			gtk_tree_model_get (model, &iter, S_SEC_COLUMN, &sec, -1);
			if (sec) {
				wireless_security_update_secrets (sec, priv->connection);
				wireless_security_unref (sec);
			}
		} while (gtk_tree_model_iter_next (model, &iter));
	}

out:
	g_object_unref (info->connection);
	g_free (info);
}

static gboolean
security_combo_init (NMAWifiDialog *self, gboolean secrets_only)
{
	NMAWifiDialogPrivate *priv;
	GtkListStore *sec_model;
	GtkTreeIter iter;
	guint32 ap_flags = 0;
	guint32 ap_wpa = 0;
	guint32 ap_rsn = 0;
	guint32 dev_caps;
	NMSettingWirelessSecurity *wsec = NULL;
	NMUtilsSecurityType default_type = NMU_SEC_NONE;
	NMWepKeyType wep_type = NM_WEP_KEY_TYPE_KEY;
	int active = -1;
	int item = 0;
	NMSettingWireless *s_wireless = NULL;
	gboolean is_adhoc;
	const char *setting_name;

	g_return_val_if_fail (self != NULL, FALSE);

	priv = NMA_WIFI_DIALOG_GET_PRIVATE (self);
	g_return_val_if_fail (priv->device != NULL, FALSE);
	g_return_val_if_fail (priv->sec_combo != NULL, FALSE);

	is_adhoc = (priv->operation == OP_CREATE_ADHOC);

	/* The security options displayed are filtered based on device
	 * capabilities, and if provided, additionally by access point capabilities.
	 * If a connection is given, that connection's options should be selected
	 * by default.
	 */
	dev_caps = nm_device_wifi_get_capabilities (NM_DEVICE_WIFI (priv->device));
	if (priv->ap != NULL) {
		ap_flags = nm_access_point_get_flags (priv->ap);
		ap_wpa = nm_access_point_get_wpa_flags (priv->ap);
		ap_rsn = nm_access_point_get_rsn_flags (priv->ap);
	}

	if (priv->connection) {
		const char *mode;

		s_wireless = nm_connection_get_setting_wireless (priv->connection);

		mode = nm_setting_wireless_get_mode (s_wireless);
		if (mode && (!strcmp (mode, "adhoc") || !strcmp (mode, "ap")))
			is_adhoc = TRUE;

		wsec = nm_connection_get_setting_wireless_security (priv->connection);

		if (wsec) {
			default_type = get_default_type_for_security (wsec, !!priv->ap, ap_flags, dev_caps);
			if (default_type == NMU_SEC_STATIC_WEP)
				wep_type = nm_setting_wireless_security_get_wep_key_type (wsec);
			if (wep_type == NM_WEP_KEY_TYPE_UNKNOWN)
				wep_type = NM_WEP_KEY_TYPE_KEY;
		}
	} else if (is_adhoc) {
		default_type = NMU_SEC_STATIC_WEP;
		wep_type = NM_WEP_KEY_TYPE_PASSPHRASE;
	}

	sec_model = gtk_list_store_new (2, G_TYPE_STRING, wireless_security_get_type ());

	if (nm_utils_security_valid (NMU_SEC_NONE, dev_caps, !!priv->ap, is_adhoc, ap_flags, ap_wpa, ap_rsn)) {
		gtk_list_store_append (sec_model, &iter);
		gtk_list_store_set (sec_model, &iter,
		                    S_NAME_COLUMN, C_("Wifi/wired security", "None"),
		                    -1);
		if (default_type == NMU_SEC_NONE)
			active = item;
		item++;
	}

	/* Don't show Static WEP if both the AP and the device are capable of WPA,
	 * even though technically it's possible to have this configuration.
	 */
	if (   nm_utils_security_valid (NMU_SEC_STATIC_WEP, dev_caps, !!priv->ap, is_adhoc, ap_flags, ap_wpa, ap_rsn)
	    && ((!ap_wpa && !ap_rsn) || !(dev_caps & (NM_WIFI_DEVICE_CAP_WPA | NM_WIFI_DEVICE_CAP_RSN)))) {
		WirelessSecurityWEPKey *ws_wep;

		ws_wep = ws_wep_key_new (priv->connection, NM_WEP_KEY_TYPE_KEY, is_adhoc, secrets_only);
		if (ws_wep) {
			add_security_item (self, WIRELESS_SECURITY (ws_wep), sec_model,
			                   &iter, _("WEP 40/128-bit Key (Hex or ASCII)"));
			if ((active < 0) && (default_type == NMU_SEC_STATIC_WEP) && (wep_type == NM_WEP_KEY_TYPE_KEY))
				active = item;
			item++;
		}

		ws_wep = ws_wep_key_new (priv->connection, NM_WEP_KEY_TYPE_PASSPHRASE, is_adhoc, secrets_only);
		if (ws_wep) {
			add_security_item (self, WIRELESS_SECURITY (ws_wep), sec_model,
			                   &iter, _("WEP 128-bit Passphrase"));
			if ((active < 0) && (default_type == NMU_SEC_STATIC_WEP) && (wep_type == NM_WEP_KEY_TYPE_PASSPHRASE))
				active = item;
			item++;
		}
	}

	/* Don't show LEAP if both the AP and the device are capable of WPA,
	 * even though technically it's possible to have this configuration.
	 */
	if (   nm_utils_security_valid (NMU_SEC_LEAP, dev_caps, !!priv->ap, is_adhoc, ap_flags, ap_wpa, ap_rsn)
	    && ((!ap_wpa && !ap_rsn) || !(dev_caps & (NM_WIFI_DEVICE_CAP_WPA | NM_WIFI_DEVICE_CAP_RSN)))) {
		WirelessSecurityLEAP *ws_leap;

		ws_leap = ws_leap_new (priv->connection, secrets_only);
		if (ws_leap) {
			add_security_item (self, WIRELESS_SECURITY (ws_leap), sec_model,
			                   &iter, _("LEAP"));
			if ((active < 0) && (default_type == NMU_SEC_LEAP))
				active = item;
			item++;
		}
	}

	if (nm_utils_security_valid (NMU_SEC_DYNAMIC_WEP, dev_caps, !!priv->ap, is_adhoc, ap_flags, ap_wpa, ap_rsn)) {
		WirelessSecurityDynamicWEP *ws_dynamic_wep;

		ws_dynamic_wep = ws_dynamic_wep_new (priv->connection, FALSE, secrets_only);
		if (ws_dynamic_wep) {
			add_security_item (self, WIRELESS_SECURITY (ws_dynamic_wep), sec_model,
			                   &iter, _("Dynamic WEP (802.1x)"));
			if ((active < 0) && (default_type == NMU_SEC_DYNAMIC_WEP))
				active = item;
			item++;
		}
	}

	if (   nm_utils_security_valid (NMU_SEC_WPA_PSK, dev_caps, !!priv->ap, is_adhoc, ap_flags, ap_wpa, ap_rsn)
	    || nm_utils_security_valid (NMU_SEC_WPA2_PSK, dev_caps, !!priv->ap, is_adhoc, ap_flags, ap_wpa, ap_rsn)) {
		WirelessSecurityWPAPSK *ws_wpa_psk;

		ws_wpa_psk = ws_wpa_psk_new (priv->connection, secrets_only);
		if (ws_wpa_psk) {
			add_security_item (self, WIRELESS_SECURITY (ws_wpa_psk), sec_model,
			                   &iter, _("WPA & WPA2 Personal"));
			if ((active < 0) && ((default_type == NMU_SEC_WPA_PSK) || (default_type == NMU_SEC_WPA2_PSK)))
				active = item;
			item++;
		}
	}

	if (   nm_utils_security_valid (NMU_SEC_WPA_ENTERPRISE, dev_caps, !!priv->ap, is_adhoc, ap_flags, ap_wpa, ap_rsn)
	    || nm_utils_security_valid (NMU_SEC_WPA2_ENTERPRISE, dev_caps, !!priv->ap, is_adhoc, ap_flags, ap_wpa, ap_rsn)) {
		WirelessSecurityWPAEAP *ws_wpa_eap;

		ws_wpa_eap = ws_wpa_eap_new (priv->connection, FALSE, secrets_only);
		if (ws_wpa_eap) {
			add_security_item (self, WIRELESS_SECURITY (ws_wpa_eap), sec_model,
			                   &iter, _("WPA & WPA2 Enterprise"));
			if ((active < 0) && ((default_type == NMU_SEC_WPA_ENTERPRISE) || (default_type == NMU_SEC_WPA2_ENTERPRISE)))
				active = item;
			item++;
		}
	}

	gtk_combo_box_set_model (GTK_COMBO_BOX (priv->sec_combo), GTK_TREE_MODEL (sec_model));
	gtk_combo_box_set_active (GTK_COMBO_BOX (priv->sec_combo), active < 0 ? 0 : (guint32) active);
	g_object_unref (G_OBJECT (sec_model));

	/* If the dialog was given a connection when it was created, that connection
	 * will already be populated with secrets.  If no connection was given,
	 * then we need to get any existing secrets to populate the dialog with.
	 */
	setting_name = priv->connection ? nm_connection_need_secrets (priv->connection, NULL) : NULL;
	if (setting_name && NM_IS_REMOTE_CONNECTION (priv->connection)) {
		GetSecretsInfo *info;

		/* Desensitize the dialog's buttons while we wait for the secrets
		 * operation to complete.
		 */
		_set_response_sensitive (self, GTK_RESPONSE_OK, FALSE);
		_set_response_sensitive (self, GTK_RESPONSE_CANCEL, FALSE);

		info = g_malloc0 (sizeof (GetSecretsInfo));
		info->self = self;
		info->connection = g_object_ref (priv->connection);
		priv->secrets_info = info;

		nm_remote_connection_get_secrets (NM_REMOTE_CONNECTION (priv->connection),
		                                  setting_name,
		                                  get_secrets_cb,
		                                  info);
	}

	return TRUE;
}

static gboolean
revalidate (gpointer user_data)
{
	NMAWifiDialog *self = NMA_WIFI_DIALOG (user_data);
	NMAWifiDialogPrivate *priv = NMA_WIFI_DIALOG_GET_PRIVATE (self);

	priv->revalidate_id = 0;
	security_combo_changed (priv->sec_combo, self);
	return FALSE;
}

static gboolean
internal_init (NMAWifiDialog *self,
               NMConnection *specific_connection,
               NMDevice *specific_device,
               gboolean secrets_only)
{
	NMAWifiDialogPrivate *priv = NMA_WIFI_DIALOG_GET_PRIVATE (self);
	GtkWidget *widget;
	char *label, *icon_name = "network-wireless";
	gboolean security_combo_focus = FALSE;

	gtk_window_set_position (GTK_WINDOW (self), GTK_WIN_POS_CENTER_ALWAYS);
	gtk_container_set_border_width (GTK_CONTAINER (self), 6);
	gtk_window_set_default_size (GTK_WINDOW (self), 488, -1);
	gtk_window_set_resizable (GTK_WINDOW (self), FALSE);

	priv->secrets_only = secrets_only;
	if (secrets_only)
		icon_name = "dialog-password";
	else
		icon_name = "network-wireless";

	gtk_window_set_icon_name (GTK_WINDOW (self), icon_name);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "image1"));
	gtk_image_set_from_icon_name (GTK_IMAGE (widget), icon_name, GTK_ICON_SIZE_DIALOG);

	gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self))), 2);

	widget = gtk_dialog_add_button (GTK_DIALOG (self), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_box_set_child_packing (GTK_BOX (gtk_dialog_get_action_area (GTK_DIALOG (self))), widget,
	                           FALSE, TRUE, 0, GTK_PACK_END);

	/* Connect/Create button */
	if (priv->operation == OP_CREATE_ADHOC) {
		GtkWidget *image;

		widget = gtk_button_new_with_mnemonic (_("C_reate"));
		image = gtk_image_new_from_stock (GTK_STOCK_CONNECT, GTK_ICON_SIZE_BUTTON);
		gtk_button_set_image (GTK_BUTTON (widget), image);

		gtk_widget_show (widget);
		gtk_dialog_add_action_widget (GTK_DIALOG (self), widget, GTK_RESPONSE_OK);
	} else {
		widget = gtk_dialog_add_button (GTK_DIALOG (self), GTK_STOCK_CONNECT, GTK_RESPONSE_OK);
		priv->ok_response_button = widget;
	}

	gtk_box_set_child_packing (GTK_BOX (gtk_dialog_get_action_area (GTK_DIALOG (self))), widget,
	                           FALSE, TRUE, 0, GTK_PACK_END);
	g_object_set (G_OBJECT (widget), "can-default", TRUE, NULL);
	gtk_widget_grab_default (widget);

	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "hbox1"));
	if (!widget) {
		g_warning ("Couldn't find Wi-Fi_dialog widget.");
		return FALSE;
	}
	gtk_widget_unparent (widget);

	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (self))), widget);

	/* If given a valid connection, hide the SSID bits and connection combo */
	if (specific_connection) {
		widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "network_name_label"));
		gtk_widget_hide (widget);

		widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "network_name_entry"));
		gtk_widget_hide (widget);

		security_combo_focus = TRUE;
		priv->network_name_focus = FALSE;
	} else {
		widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "network_name_entry"));
		g_signal_connect (G_OBJECT (widget), "changed", (GCallback) ssid_entry_changed, self);
		priv->network_name_focus = TRUE;
	}

	_set_response_sensitive (self, GTK_RESPONSE_OK, FALSE);

	if (!device_combo_init (self, specific_device)) {
		g_warning ("No Wi-Fi devices available.");
		return FALSE;
	}

	if (!connection_combo_init (self, specific_connection)) {
		g_warning ("Couldn't set up connection combo box.");
		return FALSE;
	}

	if (!security_combo_init (self, priv->secrets_only)) {
		g_warning ("Couldn't set up Wi-Fi security combo box.");
		return FALSE;
	}

	security_combo_changed (priv->sec_combo, self);
	g_signal_connect (G_OBJECT (priv->sec_combo), "changed",
	                  G_CALLBACK (security_combo_changed_manually), self);

	if (secrets_only) {
		gtk_widget_hide (priv->sec_combo);
		widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "security_combo_label"));
		gtk_widget_hide (widget);
	}

	if (security_combo_focus)
		gtk_widget_grab_focus (priv->sec_combo);
	else if (priv->network_name_focus) {
		widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "network_name_entry"));
		gtk_widget_grab_focus (widget);
	}

	if (priv->connection) {
		char *tmp;
		char *esc_ssid = NULL;
		NMSettingWireless *s_wireless;
		const GByteArray *ssid;

		s_wireless = nm_connection_get_setting_wireless (priv->connection);
		ssid = s_wireless ? nm_setting_wireless_get_ssid (s_wireless) : NULL;
		if (ssid)
			esc_ssid = nm_utils_ssid_to_utf8 (ssid);

		tmp = g_strdup_printf (_("Passwords or encryption keys are required to access the Wi-Fi network '%s'."),
		                       esc_ssid ? esc_ssid : "<unknown>");
		gtk_window_set_title (GTK_WINDOW (self), _("Wi-Fi Network Authentication Required"));
		label = g_strdup_printf ("<span size=\"larger\" weight=\"bold\">%s</span>\n\n%s",
		                         _("Authentication required by Wi-Fi network"),
		                         tmp);
		g_free (esc_ssid);
		g_free (tmp);
	} else if (priv->operation == OP_CREATE_ADHOC) {
		gtk_window_set_title (GTK_WINDOW (self), _("Create New Wi-Fi Network"));
		label = g_strdup_printf ("<span size=\"larger\" weight=\"bold\">%s</span>\n\n%s",
		                         _("New Wi-Fi network"),
		                         _("Enter a name for the Wi-Fi network you wish to create."));
	} else if (priv->operation == OP_CONNECT_HIDDEN) {
		gtk_window_set_title (GTK_WINDOW (self), _("Connect to Hidden Wi-Fi Network"));
		label = g_strdup_printf ("<span size=\"larger\" weight=\"bold\">%s</span>\n\n%s",
		                         _("Hidden Wi-Fi network"),
		                         _("Enter the name and security details of the hidden Wi-Fi network you wish to connect to."));
	} else
		g_assert_not_reached ();

	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "caption_label"));
	gtk_label_set_markup (GTK_LABEL (widget), label);
	g_free (label);

	/* Re-validate from an idle handler so that widgets like file choosers
	 * have had time to find their files.
	 */
	priv->revalidate_id = g_idle_add (revalidate, self);

	return TRUE;
}

/**
 * nma_wifi_dialog_get_connection:
 * @self: an #NMAWifiDialog
 * @device: (out):
 * @ap: (out):
 *
 * Returns: (transfer full):
 */
NMConnection *
nma_wifi_dialog_get_connection (NMAWifiDialog *self,
                                NMDevice **device,
                                NMAccessPoint **ap)
{
	NMAWifiDialogPrivate *priv;
	GtkWidget *combo;
	GtkTreeModel *model;
	WirelessSecurity *sec = NULL;
	GtkTreeIter iter;
	NMConnection *connection;
	NMSettingWireless *s_wireless;

	g_return_val_if_fail (self != NULL, NULL);

	priv = NMA_WIFI_DIALOG_GET_PRIVATE (self);

	if (!priv->connection) {
		NMSettingConnection *s_con;
		char *uuid;

		connection = nm_connection_new ();

		s_con = (NMSettingConnection *) nm_setting_connection_new ();
		uuid = nm_utils_uuid_generate ();
		g_object_set (s_con,
			      NM_SETTING_CONNECTION_TYPE, NM_SETTING_WIRELESS_SETTING_NAME,
			      NM_SETTING_CONNECTION_UUID, uuid,
			      NULL);
		g_free (uuid);
		nm_connection_add_setting (connection, (NMSetting *) s_con);

		s_wireless = (NMSettingWireless *) nm_setting_wireless_new ();
		g_object_set (s_wireless, NM_SETTING_WIRELESS_SSID, validate_dialog_ssid (self), NULL);

		if (priv->operation == OP_CREATE_ADHOC) {
			NMSettingIP4Config *s_ip4;

			g_object_set (s_wireless, NM_SETTING_WIRELESS_MODE, "adhoc", NULL);

			s_ip4 = (NMSettingIP4Config *) nm_setting_ip4_config_new ();
			g_object_set (s_ip4, NM_SETTING_IP4_CONFIG_METHOD, NM_SETTING_IP4_CONFIG_METHOD_SHARED, NULL);
			nm_connection_add_setting (connection, (NMSetting *) s_ip4);
		} else if (priv->operation == OP_CONNECT_HIDDEN) {
			/* Mark as a hidden SSID network */
			g_object_set (s_wireless, NM_SETTING_WIRELESS_HIDDEN, TRUE, NULL);
		} else
			g_assert_not_reached ();

		nm_connection_add_setting (connection, (NMSetting *) s_wireless);
	} else
		connection = g_object_ref (priv->connection);

	/* Fill security */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->sec_combo));
	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->sec_combo), &iter))
		gtk_tree_model_get (model, &iter, S_SEC_COLUMN, &sec, -1);
	if (sec) {
		wireless_security_fill_connection (sec, connection);
		wireless_security_unref (sec);
	}

	/* Save new CA cert ignore values to GSettings */
	eap_method_ca_cert_ignore_save (connection);

	/* Fill device */
	if (device) {
		combo = GTK_WIDGET (gtk_builder_get_object (priv->builder, "device_combo"));
		gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter);
		gtk_tree_model_get (priv->device_model, &iter, D_DEV_COLUMN, device, -1);
		g_object_unref (*device);
	}

	if (ap)
		*ap = priv->ap;

	return connection;
}

GtkWidget *
nma_wifi_dialog_new (NMClient *client,
                     NMRemoteSettings *settings,
                     NMConnection *connection,
                     NMDevice *device,
                     NMAccessPoint *ap,
                     gboolean secrets_only)
{
	NMAWifiDialog *self;
	NMAWifiDialogPrivate *priv;
	guint32 dev_caps;

	g_return_val_if_fail (NM_IS_CLIENT (client), NULL);
	g_return_val_if_fail (NM_IS_REMOTE_SETTINGS (settings), NULL);
	g_return_val_if_fail (NM_IS_CONNECTION (connection), NULL);

	/* Ensure device validity */
	if (device) {
		dev_caps = nm_device_get_capabilities (device);
		g_return_val_if_fail (dev_caps & NM_DEVICE_CAP_NM_SUPPORTED, NULL);
		g_return_val_if_fail (NM_IS_DEVICE_WIFI (device), NULL);
	}

	self = NMA_WIFI_DIALOG (g_object_new (NMA_TYPE_WIFI_DIALOG, NULL));
	if (self) {
		priv = NMA_WIFI_DIALOG_GET_PRIVATE (self);

		priv->client = g_object_ref (client);
		priv->settings = g_object_ref (settings);
		if (ap)
			priv->ap = g_object_ref (ap);

		priv->sec_combo = GTK_WIDGET (gtk_builder_get_object (priv->builder, "security_combo"));
		priv->group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

		/* Handle CA cert ignore stuff */
		eap_method_ca_cert_ignore_load (connection);

		if (!internal_init (self, connection, device, secrets_only)) {
			g_warning ("Couldn't create Wi-Fi security dialog.");
			gtk_widget_destroy (GTK_WIDGET (self));
			self = NULL;
		}
	}

	return GTK_WIDGET (self);
}

static GtkWidget *
internal_new_operation (NMClient *client,
                        NMRemoteSettings *settings,
                        guint operation)
{
	NMAWifiDialog *self;
	NMAWifiDialogPrivate *priv;

	g_return_val_if_fail (NM_IS_CLIENT (client), NULL);
	g_return_val_if_fail (NM_IS_REMOTE_SETTINGS (settings), NULL);

	self = NMA_WIFI_DIALOG (g_object_new (NMA_TYPE_WIFI_DIALOG, NULL));
	if (!self)
		return NULL;

	priv = NMA_WIFI_DIALOG_GET_PRIVATE (self);

	priv->client = g_object_ref (client);
	priv->settings = g_object_ref (settings);
	priv->sec_combo = GTK_WIDGET (gtk_builder_get_object (priv->builder, "security_combo"));
	priv->group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	priv->operation = operation;

	if (!internal_init (self, NULL, NULL, FALSE)) {
		g_warning ("Couldn't create Wi-Fi security dialog.");
		gtk_widget_destroy (GTK_WIDGET (self));
		return NULL;
	}

	return GTK_WIDGET (self);
}

GtkWidget *
nma_wifi_dialog_new_for_hidden (NMClient *client, NMRemoteSettings *settings)
{
	return internal_new_operation (client, settings, OP_CONNECT_HIDDEN);
}

GtkWidget *
nma_wifi_dialog_new_for_other (NMClient *client, NMRemoteSettings *settings)
{
	return internal_new_operation (client, settings, OP_CONNECT_HIDDEN);
}

GtkWidget *
nma_wifi_dialog_new_for_create (NMClient *client, NMRemoteSettings *settings)
{
	return internal_new_operation (client, settings, OP_CREATE_ADHOC);
}

/**
 * nma_wifi_dialog_nag_user:
 * @self:
 *
 * Returns: (transfer full):
 */
GtkWidget *
nma_wifi_dialog_nag_user (NMAWifiDialog *self)
{
	return NULL;
}

static void
nma_wifi_dialog_init (NMAWifiDialog *self)
{
	NMAWifiDialogPrivate *priv = NMA_WIFI_DIALOG_GET_PRIVATE (self);
	GError *error = NULL;

	priv->builder = gtk_builder_new ();

	if (!gtk_builder_add_from_file (priv->builder, UIDIR "/wifi.ui", &error)) {
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
	}
}

static void
dispose (GObject *object)
{
	NMAWifiDialogPrivate *priv = NMA_WIFI_DIALOG_GET_PRIVATE (object);

	if (priv->disposed) {
		G_OBJECT_CLASS (nma_wifi_dialog_parent_class)->dispose (object);
		return;
	}

	priv->disposed = TRUE;

	if (priv->secrets_info)
		priv->secrets_info->canceled = TRUE;

	g_object_unref (priv->client);
	g_object_unref (priv->settings);
	g_object_unref (priv->builder);

	g_object_unref (priv->device_model);
	g_object_unref (priv->connection_model);

	if (priv->group)
		g_object_unref (priv->group);

	if (priv->connection)
		g_object_unref (priv->connection);

	if (priv->device)
		g_object_unref (priv->device);

	if (priv->ap)
		g_object_unref (priv->ap);

	if (priv->revalidate_id)
		g_source_remove (priv->revalidate_id);

	G_OBJECT_CLASS (nma_wifi_dialog_parent_class)->dispose (object);
}

static void
nma_wifi_dialog_class_init (NMAWifiDialogClass *nmad_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (nmad_class);

	g_type_class_add_private (nmad_class, sizeof (NMAWifiDialogPrivate));

	/* virtual methods */
	object_class->dispose = dispose;
}
